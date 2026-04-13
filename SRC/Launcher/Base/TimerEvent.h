#pragma once
#include <functional>
#include <chrono>
#include <deque>
#include <memory>

namespace Event {

	namespace Detail {
		class StaticManager;

		inline double GetTimeSeconds() {
			//using namespace std;
			using namespace std::chrono;
			return duration<double>(
				high_resolution_clock::now().time_since_epoch()).count();
		}


		class EventBase {
			friend class ::Event::Detail::StaticManager;
		public:
			virtual ~EventBase() {}
		protected:
			virtual void _RunEvent() = 0;
			bool _IsExpired() const {
				return GetTimeSeconds() > _NextTime;
			}

		private:
			double _NextTime = 0.0;
		};

		class StaticManager {
		public:
			using HandleBase = std::shared_ptr<Detail::EventBase>;

		private:
			static inline std::deque<HandleBase> _EventDeq;
			static inline std::deque<HandleBase> _InsertDeq;
			static inline std::deque<HandleBase> _DeleteDeq;

		public:

			template<class EventType>
			static HandleBase AddEvent(double Seconds, const EventType& Event) {
				static_assert(std::is_convertible_v<EventType*, EventBase*>);
				auto Ins = std::make_shared<EventType>(Event); 
				Ins->_NextTime = GetTimeSeconds() + Seconds;
				_InsertDeq.emplace_back(Ins);
				return Ins;
			}

			static void Flush() {
				//adding new handles
				for (auto& Handle : _InsertDeq)
					_EventDeq.emplace_back(Handle);
				_InsertDeq.clear();

				//removing deleted handles
				for (auto& Handle : _DeleteDeq)
				{
					if (auto Iter = std::ranges::find(_EventDeq, Handle);
						Iter != _EventDeq.end()) {
						_EventDeq.erase(Iter);
					}
				}
				_DeleteDeq.clear();

				//flushing events
				for (auto it = _EventDeq.begin(); it != _EventDeq.end();) {
					if ((*it)->_IsExpired()) {
						auto Handle = *it;
						Handle->_RunEvent();
						it = _EventDeq.erase(it);
						continue;
					} ++it;
				}
			}

			static void DeleteEvent(const HandleBase& Handle) {
				_DeleteDeq.emplace_back(Handle);
			}
		};

		struct EmptyInfo {  };
	}

	template<typename Info = Detail::EmptyInfo>
	class Event : public Info, public Detail::EventBase {
	public:
		using EventFunc = std::function<void(const Info&)>;
		using Handle = std::shared_ptr<Event<Info>>;

		Event() {}
		Event(const Event<Info>& Other) : Info(Other._AsInfo()) {
			_Func = Other._Func;
		}
		void SetEvent(const EventFunc& Func) {
			_Func = Func;
		}

		static Event<Info> Make(const EventFunc& Func) {
			Event<Info> Even;
			Even.SetEvent(Func);
			return Even;
		}

	private:
		const Info& _AsInfo() const { return *reinterpret_cast<const Info*>(this); }

	protected:
		void _RunEvent() override {
			if (_Func)
				_Func(_AsInfo());
		}

	private:
		EventFunc _Func;
	};

	using EventHandle = Detail::StaticManager::HandleBase;

	template<class Event>
	decltype(auto) AddEvent(double Seconds, const Event& _Event) {
		return Detail::StaticManager::AddEvent<Event>(Seconds, _Event);
	}

	inline void DeleteEvent(const EventHandle& Handle) {
		Detail::StaticManager::DeleteEvent(Handle);
	}

	inline void Flush() {
		Detail::StaticManager::Flush();
	}

	template<typename EventType>
	EventType* HandleCast(const EventHandle& Handle) {
		return dynamic_cast<EventType*>(Handle.get());
	}
}