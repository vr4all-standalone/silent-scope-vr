#pragma once
#include <string>
struct StubSettings{struct Content{std::string gameId;}content;};inline StubSettings settings;
enum class Event{Start};struct EventManager{template<class F>static void listen(Event,F){}};
