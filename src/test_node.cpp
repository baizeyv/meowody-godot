//
// Created by baizeyv on 8/22/2026.
//

#include "test_node.h"
#include <godot_cpp/variant/utility_functions.hpp>

void TestNode::say_hello()
{
    UtilityFunctions::print("hello from cpp!aa");
}

Vector2 TestNode::offset(Vector2 value) const
{
    
    // meowody::Runtime::events().connect<Evt>(&test);
    
    return value + Vector2(1.0, 1.0);
}

void TestNode::test()
{
}

