//                  ReactivePlusPlus library
//
//          Copyright Aleksey Loginov 2022 - present.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/victimsnino/ReactivePlusPlus
//

#include <snitch/snitch.hpp>
#include <rpp/sources/create.hpp>
#include <exception>
#include "mock_observer.hpp"
#include "rpp/disposables/composite_disposable.hpp"

TEST_CASE("subscribe as operator")
{
    mock_observer_strategy<int> mock{};
    auto observable = rpp::source::create<int>([](const auto& obs)
    { 
        obs.on_next(1);
        obs.on_completed();
    });
    
    SECTION("subscribe observer")
    {
        static_assert(std::is_same_v<decltype(observable | rpp::operators::subscribe(mock.get_observer())), void>);
        observable | rpp::operators::subscribe(mock.get_observer());
        CHECK(mock.get_received_values() == std::vector{1});
    }
    
    SECTION("subscribe observer with disposable")
    {
        static_assert(std::is_same_v<decltype(observable | rpp::operators::subscribe(rpp::composite_disposable{}, mock.get_observer())), rpp::composite_disposable>);
        auto d = observable | rpp::operators::subscribe(rpp::composite_disposable{}, mock.get_observer());
        CHECK(d.is_disposed());
        CHECK(mock.get_received_values() == std::vector{1});
    }
    
    SECTION("subscribe lambdas")
    {
        static_assert(std::is_same_v<decltype(observable | rpp::operators::subscribe([](const auto&){}, [](const std::exception_ptr&){}, [](){})), void>);
        observable | rpp::operators::subscribe([&mock](const auto& v){ mock.on_next(v);}, [](const std::exception_ptr&){}, [](){});
        CHECK(mock.get_received_values() == std::vector{1});
    }

    SECTION("subscribe lambdas with disposable")
    {
        static_assert(std::is_same_v<decltype(observable | rpp::operators::subscribe(rpp::composite_disposable{}, [](const auto&){}, [](const std::exception_ptr&){}, [](){})), rpp::composite_disposable>);
        auto d = observable | rpp::operators::subscribe(rpp::composite_disposable{}, [&mock](const auto& v){ mock.on_next(v);}, [](const std::exception_ptr&){}, [](){});
        CHECK(d.is_disposed());
        CHECK(mock.get_received_values() == std::vector{1});
    }
}