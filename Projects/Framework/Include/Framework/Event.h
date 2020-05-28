#pragma once

#include <functional>


class Action
{
private:
	std::vector<std::function<void()>*> _receivers = {};

public:
	void Attach(std::function<void()>& receiver)
	{
		_receivers.push_back(&receiver);
	}

	void Detach(std::function<void()>& receiver)
	{
		const auto it = std::find(_receivers.begin(), _receivers.end(), &receiver);
		if (it != _receivers.end()) {
			_receivers.erase(it);
		}
	}

	void Invoke()
	{
		for (auto* r : _receivers) {
			(*r)();
		}
	}
};


template<class... TArgs>
class Event
{
private:
	std::vector<std::function<void(TArgs...)>*> _receivers = {};

public:
	void Attach(std::function<void(TArgs...)>& receiver)
	{
		_receivers.push_back(&receiver);
	}

	void Detach(std::function<void(TArgs...)>& receiver)
	{
		const auto it = std::find(_receivers.begin(), _receivers.end(), &receiver);
		if (it != _receivers.end()) {
			_receivers.erase(it);
		}
	}

	void Invoke(TArgs... args)
	{
		for (auto* r : _receivers) {
			(*r)(args...);
		}
	}
};
