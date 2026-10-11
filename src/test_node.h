//
// Created by baizeyv on 8/22/2026.
//

#ifndef GDMEOWODYFRAMEWORK_TEST_NODE_H
#define GDMEOWODYFRAMEWORK_TEST_NODE_H

#include <godot_cpp/classes/node.hpp>

#include "reflection/annotations.h"
#include "reflection/class_macros.h"
#include "reflection/gs.h"

using namespace godot;

class TestNode : public Node {
	GDREFLECT_CLASS(TestNode, Node);

public:
	TestNode() = default;
	~TestNode() override = default;

	GDPROPERTY_CATEGORY("Core")
	GDPROPERTY_GROUP("Movement", "")
	GDPROPERTY_USAGE(PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED)
	GDPROPERTY_RANGE(0, 240, 1)
	int speed = 100;

	GDPROPERTY
	int a = 2;

	GDPROPERTY
	Rect2i test_rect;

	GDRPC(MultiplayerAPI::RPC_MODE_AUTHORITY, MultiplayerPeer::TRANSFER_MODE_RELIABLE, true, 0)
	GDMETHOD
	void say_hello();

	GDMETHOD
	[[nodiscard]] Vector2 offset(Vector2 value) const;

	GDSIGNAL
	void speed_changed(int value);

	GS(int, hp)

	struct Evt {};

	static void test();
};

#endif //GDMEOWODYFRAMEWORK_TEST_NODE_H
