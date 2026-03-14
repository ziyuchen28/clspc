#pragma once 

#include "test_helper.h"


inline std::string real_jdtls_home() 
{
    if (const char *env = std::getenv("CLSPC_TEST_JDTLS_HOME")) {
        return env;
    }
    require(false, "set CLSPC_TEST_JDTLS_HOME to the extracted JDTLS directory");
    return {};
}

inline std::string real_java_bin() 
{
    if (const char *env = std::getenv("CLSPC_TEST_JAVA_BIN")) {
        return env;
    }
    return "java";
}

inline std::string shell_quote_single(std::string_view s) 
{
    std::string out;
    out.push_back('\'');
    for (char ch : s) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('\'');
    return out;
}
