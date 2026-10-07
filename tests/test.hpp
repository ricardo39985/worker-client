#pragma once
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
struct Test { const char* name; std::function<void()> run; };
inline std::vector<Test>& tests(){static std::vector<Test> t;return t;}
struct Register { Register(const char* n,std::function<void()> f){tests().push_back({n,std::move(f)});} };
#define JOIN_(a,b) a##b
#define JOIN(a,b) JOIN_(a,b)
#define TEST(name) static void JOIN(test_,__LINE__)(); static Register JOIN(reg_,__LINE__)(name,JOIN(test_,__LINE__)); static void JOIN(test_,__LINE__)()
#define CHECK(...) do{if(!(__VA_ARGS__))throw std::runtime_error(std::string("check failed at ")+__FILE__+":"+std::to_string(__LINE__)+": " #__VA_ARGS__);}while(false)
#define THROWS(expr) do{bool caught=false;try{expr;}catch(const std::exception&){caught=true;}CHECK(caught);}while(false)
