#
#  rhom_object_factory.rb
#  rhodes
#  Returns an array of RhomObjects
#
#  Copyright (C) 2008 Lars Burgess. All rights reserved.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
require 'rhom_object'

class RhomObjectFactory
  attr_accessor :obj_list, :classnames, :attrib_count
  
  def initialize
    init_objects unless not defined? RHO_SOURCES
  end
  
  # setup the sources table and model attributes for this application
  def self.init_sources
    if defined? RHO_SOURCES
      attribs = {}
      Rhom::execute_sql "delete from sources"
      src_attribs = []
      attribs_empty = false
      RHO_SOURCES.each do |source, id|
        Rhom::execute_sql "insert into sources (source_id) values (#{id.to_i})"
        src_attribs = Rhom::execute_sql "select count(distinct attrib) as count 
                                           from object_values where source_id=#{id.to_i}"
        attribs[source] = src_attribs[0]['count'].to_i
        # there are no records yet, raise a flag so we don't define the constant
        if attribs[source] == 0
          attribs_empty = true
        end
      end
    end
    Object::const_set("SOURCE_ATTRIBS", attribs) unless defined? SOURCE_ATTRIBS or attribs_empty
  end
  
  # Initialize new object with dynamic attributes
  def init_objects
    RHO_SOURCES.each do |classname,source|
      unless Object::const_defined?(classname.intern)
        Object::const_set(classname.intern, 
          Class::new do
            include RhomObject
            extend RhomObject
          
            def initialize(obj=nil)
              if obj
                # create a temp id for the create type
                # TODO: This is duplicative of get_new_obj
                temp_objid = djb_hash(obj.values.to_s, 10).to_s
                self.send 'object'.to_sym, "#{temp_objid}"
                self.send 'source_id'.to_sym, obj['source_id'].to_s
                self.send 'update_type'.to_sym, 'create'
                obj.each do |key,value|
                  self.send key, value
                end
              end
            
            end
		  
            class << self
              
              def get_source_id
                RHO_SOURCES[self.name.to_s].to_s
              end
              # retrieve a single record if object id provided, otherwise return
              # full list corresponding to factory's source id
              def find(*args)
                list = []
                if args.first == :all
                  query = "select * from #{TABLE_NAME} where \
                           source_id=#{get_source_id} \
                           and update_type in ('create', 'query') \
                           order by object"
                else
                  obj = strip_braces(args.first.to_s)
                  query = "select * from #{TABLE_NAME} where object='#{obj}'"
                end
                result = Rhom::execute_sql(query)
                list = get_list(result)
                if list.length == 1
                  return list[0]
                end
                list
              end
              
              def find_by(*args)
                # TODO: implement
              end
    
              def find_by_sql(sql)
                result = Rhom::execute_sql(sql)
                get_list(result)
              end
            
              # returns an array of objects based on an existing array
              def get_list(objs)
                new_list = []
                if objs and defined? SOURCE_ATTRIBS
                  attrib_length = SOURCE_ATTRIBS[self.name.to_s]
                  list_length = 0
                  list_length = (objs.length / attrib_length) unless attrib_length == 0
                  new_obj = nil
                  # iterate over the array and determine object
                  # structure based on attribute/value pairs
                  list_length.times do |i|
                    new_obj = get_new_obj(objs[i*attrib_length])
                    attrib_length.times do |j|
                      # setup index and assign accessors
                      idx = i*attrib_length+j
                      begin
                        # only update attributes if they belong
                        # to the current object
                        if objs[idx]['object'] == strip_braces((new_obj.send 'object'.to_sym))
                          attrib = objs[idx]['attrib'].to_s
                          value = objs[idx]['value'].to_s
                          new_obj.send attrib.to_sym, value
                        end
                      rescue
                        puts "failed to reference objs[#{idx}]..."
                      end
                    end
                    new_list << new_obj
                  end
                else
                  # source attributes are not initialized, 
                  # try again
                  RhomObjectFactory::init_sources
                end
                new_list
              end
  
              # returns new model instance with a temp object id
              def get_new_obj(obj, type='query')
                tmp_obj = self.new
                tmp_obj.send 'object'.to_sym, "{#{obj['object'].to_s}}"
                tmp_obj.send 'source_id'.to_sym, get_source_id
                tmp_obj.send 'update_type'.to_sym, type
                tmp_obj
              end
            end #class methods
	
            # deletes the record from the viewable list as well as
            # adding a delete record to the list of sync operations
            def destroy
              result = nil
              obj = strip_braces(self.object)
              if obj
                # first delete the record from viewable list
                query = "delete from #{TABLE_NAME} where object='#{obj}'"
                result = Rhom::execute_sql(query)
                # now add delete operation
                query = "insert into #{TABLE_NAME} (source_id, object, update_type) \
                         values (#{self.get_inst_source_id}, '#{obj}', 'delete')"
                result = Rhom::execute_sql(query)
              end
              result
            end
		
            # saves the current object to the database as a create type
            def save
              result = nil
              # iterate over each instance variable and insert create row to table
              self.instance_variables.each do |method|
                method = method.to_s.gsub(/@/,"")
                # Don't save objects with braces to database
                val = strip_braces(self.send(method.to_sym))
                # add rows excluding object, source_id and update_type
                unless self.method_name_reserved?(method) or val.nil? or val.length == 0
                  query = "insert into #{TABLE_NAME} (source_id, object, attrib, value, update_type) values \
                          (#{self.get_inst_source_id}, '#{self.object}', '#{method}', '#{val}', 'create')"
                  result = Rhom::execute_sql(query)
                end
              end
              result
            end
          
            # updates the current record in the viewable list and adds
            # a sync operation to update
            def update_attributes(attrs)
              result = nil
              obj = strip_braces(self.object)
              self.instance_variables.each do |method|
                method = method.to_s.gsub(/@/,"")
                val = self.send method.to_sym
                # Don't save objects with braces to database
                new_val = strip_braces(attrs[method])
                # if the object's value doesn't match the database record
                # then we procede with update
                if new_val and val != new_val
                  unless self.method_name_reserved?(method) or new_val.length == 0
                    # update viewable list
                    query = "update #{TABLE_NAME} set value='#{new_val}' where object='#{obj}' \
			   and attrib='#{method}'"
                    result = Rhom::execute_sql(query)
                    # update sync list
                    query = "insert into #{TABLE_NAME} (source_id, object, attrib, value, update_type) values \
                           (#{self.get_inst_source_id}, '#{obj}', '#{method}', '#{new_val}', 'update')"
                    result = Rhom::execute_sql(query)
                  end
                end
              end
              result
            end
	
            def get_inst_source_id
              RHO_SOURCES[self.class.name.to_s].to_s
            end
            
            def method_name_reserved?(method)
              method =~ /id|object|source_id|update_type/
            end
          end)
      end
    end
  end
end