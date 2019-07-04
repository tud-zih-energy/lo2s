#include <lo2s/java/jni_signature.hpp>

#include <iostream>

int main()
{
    // std::cout << lo2s::java::parse_signature("()Ljava/lang/String;") << std::endl;

    for (auto arg : lo2s::java::parse_type_list("Ljava/lang/Object;Ljava/lang/ref/Cleaner;"))
    {
        std::cout << arg << std::endl;
    }
}
