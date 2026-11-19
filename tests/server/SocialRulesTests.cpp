#include "../../SRC/Server/GameServer/core/SocialRules.hpp"
#include <stdexcept>
#include <iostream>
int main(){using namespace auriga::social;auto check=[](bool v){if(!v)throw std::runtime_error("Social name boundary");};check(ValidName("Luna"));check(ValidName("[ADMIN]Razor"));check(ValidName(std::string(24,'a')));for(auto s:{"","Two Names","Luna\n","\tName","Name\x7f"})check(!ValidName(s));check(!ValidName(std::string(25,'a')));check(PendingLimit==64 && ContactLimit==256);std::cout<<"SOCIAL_RULES=PASS\n";}
