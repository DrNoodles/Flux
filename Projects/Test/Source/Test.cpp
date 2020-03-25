#include "Test.h"

Test2D::Test2D()
{
	_id = ++_testCount;
}

int Test2D::GetId() const { return _id; }
