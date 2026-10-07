#include "test.hpp"
int main(){unsigned failures=0;for(const auto& t:tests()){try{t.run();std::cout<<"PASS "<<t.name<<'\n';}catch(const std::exception& e){++failures;std::cout<<"FAIL "<<t.name<<": "<<e.what()<<'\n';}}std::cout<<tests().size()<<" contracts, "<<failures<<" failures\n";return failures?1:0;}
