require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/../../spec_helper'
require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/shared/include.rb'

describe "ENV.key?" do
  it_behaves_like(:env_include, :key?)
end
