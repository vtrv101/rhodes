# XXX eval not supported
#require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/../../spec_helper'
#require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/fixtures/classes'
#
#describe "Kernel#instance_eval" do
#  it "expects a block with no arguments" do
#    lambda { "hola".instance_eval }.should raise_error(ArgumentError)
#  end
#
#  it "takes no arguments with a block" do
#    lambda { "hola".instance_eval(4, 5) { |a,b| a + b } }.should raise_error(ArgumentError)
#  end
#
#  it "passes the object to the block" do
#    "hola".instance_eval { |o| o.size }.should == 4
#  end
#
#  it "only binds the eval to the receiver" do
#    f = Object.new
#    f.instance_eval do
#      def foo
#        1
#      end
#    end
#    f.foo.should == 1
#    lambda { Object.new.foo }.should raise_error(NoMethodError)
#  end
#
#  # TODO: This should probably be replaced with a "should behave like" that uses
#  # the many scoping/binding specs from kernel/eval_spec, since most of those
#  # behaviors are the same for instance_eval. See also module_eval/class_eval.
#  it "shares a scope across sibling evals" do
#    a, b = Object.new, Object.new
#
#    result = nil
#    a.instance_eval "x = 1"
#    lambda do
#      b.instance_eval "result = x"
#    end.should_not raise_error
#    result.should == 1
#  end
#
#  it "binds self to the receiver" do
#    s = "hola"
#    (s == s.instance_eval { self }).should == true
#    o = mock('o')
#    (o == o.instance_eval("self")).should == true
#  end
#
#  it "executes in the context of the receiver" do
#    "Ruby-fu".instance_eval { size }.should == 7
#    "hola".instance_eval("size").should == 4
#    Object.class_eval { "hola".instance_eval("to_s") }.should == "hola"
#    Object.class_eval { "Ruby-fu".instance_eval{ to_s } }.should == "Ruby-fu"
#
#  end
#
#  it "has access to receiver's instance variables" do
#    KernelSpecs::IVars.new.instance_eval { @secret }.should == 99
#    KernelSpecs::IVars.new.instance_eval("@secret").should == 99
#  end
#
#  it "treats block-local variables as local to the block" do
#    prc = instance_eval <<-CODE
#      proc do |x, prc|
#        if x
#          n = 2
#        else
#          n = 1
#          prc.call(true, prc)
#          n
#        end
#      end
#    CODE
#
#    prc.call(false, prc).should == 1
#  end
#
#  it "sets class variables in the receiver" do
#    KernelSpecs::IncludesInstEval.class_variables.should include("@@count")
#    KernelSpecs::IncludesInstEval.send(:class_variable_get, :@@count).should == 2
#  end
#
#  it "raises a TypeError when defining methods on an immediate" do
#    lambda do
#      1.instance_eval { def foo; end }
#    end.should raise_error(TypeError)
#    lambda do
#      :foo.instance_eval { def foo; end }
#    end.should raise_error(TypeError)
#  end
#
#quarantine! do # Not clean, leaves cvars lying around to break other specs
#  it "scopes class var accesses in the caller when called on a Fixnum" do
#    # Fixnum can take instance vars
#    Fixnum.class_eval "@@__tmp_instance_eval_spec = 1"
#    (defined? @@__tmp_instance_eval_spec).should == nil
#
#    @@__tmp_instance_eval_spec = 2
#    1.instance_eval { @@__tmp_instance_eval_spec }.should == 2
#    Fixnum.__send__(:remove_class_variable, :@@__tmp_instance_eval_spec)
#  end
#end
#
#  it "raises a TypeError when defining methods on numerics" do
#    lambda do
#      (1.0).instance_eval { def foo; end }
#    end.should raise_error(TypeError)
#    lambda do
#      (1 << 64).instance_eval { def foo; end }
#    end.should raise_error(TypeError)
#  end
#end
