#pragma once

class Test2D
{
public:
	Test2D();
	int GetId() const;

private:
	int _id = -1;
	inline static int _testCount = -1;
};