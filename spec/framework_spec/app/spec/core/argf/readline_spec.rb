require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/../../spec_helper'
require File.dirname(File.join(__rhoGetCurrentDir(), __FILE__)) + '/shared/gets'

describe "ARGF.readline" do
  it_behaves_like :argf_gets, :readline
end

describe "ARGF.readline" do
  it_behaves_like :argf_gets_inplace_edit, :readline
end

describe "ARGF.readline" do
  before :each do
    @file1 = fixture File.join(__rhoGetCurrentDir(), __FILE__), "file1.txt"
    @file2 = fixture File.join(__rhoGetCurrentDir(), __FILE__), "file2.txt"
  end

  after :each do
    ARGF.close
  end

  it "raises an EOFError when reaching end of files" do
    argv [@file1, @file2] do
      lambda { while line = ARGF.readline; end }.should raise_error(EOFError)
    end
  end
end
