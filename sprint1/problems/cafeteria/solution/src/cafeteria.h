#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <atomic>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Вспомогательная структура для хранения состояния заказа
struct OrderState {
    std::shared_ptr<Sausage> sausage;
    std::shared_ptr<Bread> bread;
    std::shared_ptr<std::atomic<int>> ready_count;
    std::shared_ptr<std::exception_ptr> error;
    HotDogHandler handler;
};

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria : public std::enable_shared_from_this<Cafeteria> {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        // Получаем ингредиенты со склада
        auto sausage = store_.GetSausage();
        auto bread = store_.GetBread();
        
        // Создаем состояние заказа
        auto state = std::make_shared<OrderState>();
        state->sausage = std::move(sausage);
        state->bread = std::move(bread);
        state->ready_count = std::make_shared<std::atomic<int>>(0);
        state->error = std::make_shared<std::exception_ptr>();
        state->handler = std::move(handler);
        
        // Создаем strand для этого заказа
        auto strand = std::make_shared<net::strand<net::io_context::executor_type>>(
            net::make_strand(io_));
        
        // Запускаем приготовление в strand
        net::dispatch(*strand, [
            self = shared_from_this(),
            state,
            strand,
            gas_cooker = gas_cooker_,
            &io = io_
        ]() mutable {
            // Функция завершения заказа
            auto complete_order = [state, &io] {
                if (*state->ready_count == 2) {
                    // Оба ингредиента готовы
                    try {
                        static std::atomic<int> next_id{1};
                        HotDog hot_dog{next_id++, state->sausage, state->bread};
                        
                        net::post(io, 
                            [handler = std::move(state->handler), 
                             hot_dog = std::move(hot_dog)]() mutable {
                            handler(std::move(hot_dog));
                        });
                    } catch (...) {
                        net::post(io,
                            [handler = std::move(state->handler),
                             e = std::current_exception()] {
                            handler(e);
                        });
                    }
                } else if (*state->error) {
                    // Была ошибка
                    net::post(io,
                        [handler = std::move(state->handler),
                         error = state->error] {
                        handler(*error);
                    });
                }
            };
            
            // Приготовление сосиски
            state->sausage->StartFry(*gas_cooker, [
                state,
                strand,
                complete_order,
                &io = io
            ] {
                auto timer = std::make_shared<net::steady_timer>(
                    io,
                    std::chrono::milliseconds(1500));
                
                timer->async_wait([
                    timer,
                    state,
                    complete_order
                ](boost::system::error_code ec) {
                    if (ec) {
                        *state->error = std::make_exception_ptr(
                            std::runtime_error("Sausage timer error"));
                        complete_order();
                        return;
                    }
                    
                    try {
                        state->sausage->StopFry();
                        ++(*state->ready_count);
                        complete_order();
                    } catch (...) {
                        *state->error = std::current_exception();
                        complete_order();
                    }
                });
            });
            
            // Приготовление булки
            state->bread->StartBake(*gas_cooker, [
                state,
                strand,
                complete_order,
                &io = io
            ] {
                auto timer = std::make_shared<net::steady_timer>(
                    io,
                    std::chrono::milliseconds(1000));
                
                timer->async_wait([
                    timer,
                    state,
                    complete_order
                ](boost::system::error_code ec) {
                    if (ec) {
                        *state->error = std::make_exception_ptr(
                            std::runtime_error("Bread timer error"));
                        complete_order();
                        return;
                    }
                    
                    try {
                        state->bread->StopBaking();
                        ++(*state->ready_count);
                        complete_order();
                    } catch (...) {
                        *state->error = std::current_exception();
                        complete_order();
                    }
                });
            });
        });
    }

private:
    net::io_context& io_;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};