require 'json'
require 'mechanize'
require 'zip/zip'

namespace "rhosync" do
  task :config => "config:common" do
    $url = 'http://localhost:9292'
    $agent = WWW::Mechanize.new
    $appname = $app_basedir.gsub(/\\/, '/').split('/').last
    $token_file = File.join(ENV['HOME'],'.rhosync_token')
    $token = File.read($token_file) if File.exist?($token_file)
  end
  
  desc "Fetches current api token from rhosync"
  task :get_api_token => :config do
    login = ask "Login: "
    password = ask "Password: "
    $agent.post("#{$url}/login", :login => login, :password => password)
    res = ask "Save token (#{$token_file}):"
    $token_file = res if res.length > 0
    $token = $agent.post("#{$url}/api/get_api_token").body
    File.open($token_file,'w') {|f| f.write $token}
  end
  
  desc "Imports an application into rhosync"
  task :import_app => :config do
    name = File.join($app_basedir,'rhosync')
    compress(name)
    $agent.post("#{$url}/api/import_app",
      :app_name => $appname, :api_token => $token,
      :upload_file =>  File.new(File.join($app_basedir,'rhosync','rhosync.zip'), "rb"))
    FileUtils.rm archive(name), :force => true
  end
  
  desc "Deletes an application from rhosync"
  task :delete_app => :config do
    $agent.post("#{$url}/api/delete_app", :app_name => $appname, :api_token => $token)
  end
  
  desc "Creates and subscribes user for application in rhosync"
  task :create_user => :config do
    login = ask "login: "
    password = ask "password: "
    $agent.post("#{$url}/api/create_user", :app_name => $appname, :api_token => $token,
      :attributes => {:login => login, :password => password})
  end
end

def archive(path)
  File.join(path,File.basename(path))+'.zip'
end

def ask(msg)
  print msg
  STDIN.gets.chomp
end

def compress(path)
  path.sub!(%r[/$],'')
  FileUtils.rm archive(path), :force=>true
  Zip::ZipFile.open(archive(path), 'w') do |zipfile|
    Dir["#{path}/**/**"].reject{|f|f==archive(path)}.each do |file|
      zipfile.add(file.sub(path+'/',''),file)
    end
    zipfile.close
  end
end