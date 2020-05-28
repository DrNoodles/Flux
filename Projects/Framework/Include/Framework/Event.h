#pragma once

#include <unordered_set>


// TODO Look into using ...

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class IEventReceiver
{
public:
	virtual ~IEventReceiver() = default;
	virtual void Invoke() = 0;
};

class EventSource
{
private:
	std::unordered_set<IEventReceiver*> _receivers;
	
public:
	void Attach(IEventReceiver& receiver) {
		_receivers.insert(&receiver);
	}

	void Detach(IEventReceiver& receiver) {
		_receivers.extract(&receiver);
	}

	void Invoke() const 	{
		for (auto* reciever : _receivers) {
			reciever->Invoke();
		}
	}
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template<class TSender, class TArgs>
class ITypedEventReceiver
{
public:
	virtual ~ITypedEventReceiver() = default;
	virtual void Invoke(TSender s, TArgs a) = 0;
};

template<class TSender, class TArgs>
class TypedEventSource
{
private:
	std::unordered_set<ITypedEventReceiver<TSender, TArgs>*> _receivers;
	
public:

	void Attach(const ITypedEventReceiver<TSender, TArgs>& receiver)
	{
		_receivers.insert({&receiver, receiver});
	}

	void Detach(const ITypedEventReceiver<TSender, TArgs>& receiver)
	{
		_receivers.extract(&receiver);
	}

	void Invoke(TSender s, TArgs a)
	{
		for (auto* reciever : _receivers) {
			reciever->Invoke(s, a);
		}
	}

};
