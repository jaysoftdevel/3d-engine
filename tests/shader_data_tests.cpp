#include "../src/smaa_shader.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char**argv){
 if(argc!=2)return 2;
 std::ifstream f(argv[1],std::ios::binary);
 if(!f){std::cerr<<"Cannot read bundled SMAA source\n";return 2;}
 std::string original((std::istreambuf_iterator<char>(f)),{}),normalized;
 for(size_t i=0;i<original.size();i++){
  if(original[i]=='\r'&&i+1<original.size()&&original[i+1]=='\n')continue;
  normalized+=original[i];
 }
 if(normalized!=smaaReference){std::cerr<<"Embedded SMAA source changed or was truncated\n";return 1;}
 std::cout<<"Complete SMAA source preserved: "<<smaaReference.size()<<" bytes\n";
}
