#pragma once

#include <unordered_set>
#include <functional>


// TODO Look into using ...

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//class IEventReceiver
//{
//public:
//	virtual ~IEventReceiver() = default;
//	virtual void Invoke() = 0;
//};
//
//class EventSource
//{
//private:
//	std::unordered_set<IEventReceiver*> _receivers;
//	
//public:
//	void Attach(IEventReceiver& receiver) {
//		_receivers.insert(&receiver);
//	}
//
//	void Detach(IEventReceiver& receiver) {
//		_receivers.extract(&receiver);
//	}
//
//	void Invoke() const 	{
//		for (auto* reciever : _receivers) {
//			reciever->Invoke();
//		}
//	}
//};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*template<class TSender, class TArgs>
class ITypedEventReceiver
{
public:
	virtual ~ITypedEventReceiver() = default;
	virtual void Invoke(TSender s, TArgs a) = 0;
};*/

template<class TSender, class TArgs>
class TypedEventDelegate
{
private:
	using fn = std::function<void(TSender, TArgs)>;
	fn _func;

public:
	TypedEventDelegate(fn func) : _func(func) {}
	void operator()(TSender s, TArgs a)
	{
		_func(s, a);
	}
};


template<class TSender, class TArgs>
class TypedEvent
{

private:
	std::list<TypedEventDelegate<TSender, TArgs>*> _bindings;

	
public:
	void Attach(TypedEventDelegate<TSender, TArgs>* receiver)
	{
		_bindings.push_back(receiver);
	}

	void Detach(TypedEventDelegate<TSender, TArgs>* receiver)
	{
		auto it = std::find(_bindings.begin(), _bindings.end(), receiver);
		if (it != _bindings.end()) {
			_bindings.erase(it);
		}
	}

	void Invoke(TSender s, TArgs a)
	{
		for (auto b : _bindings) {
			auto bb = (*b);
			bb(s, a);
		}
	}
};
