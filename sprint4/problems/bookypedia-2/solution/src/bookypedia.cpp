#include "bookypedia.h"
#include <iostream>
#include "menu/menu.h"
#include "ui/view.h"
#include <string>

using namespace std::string_literals;

namespace bookypedia {

Application::Application(const AppConfig& config)
    : db_{pqxx::connection{config.db_url}} {
}

void Application::Run() {
    menu::Menu menu{std::cin, std::cout};

    menu.AddAction("Help"s, {}, "Show instructions"s, [&menu](std::istream&) {
        menu.ShowInstructions();
        return true;
    });

    menu.AddAction("Exit"s, {}, "Exit program"s, [&menu](std::istream&) {
        return false;
    });

    // Создаем Unit of Work для каждого запроса
    app::UseCasesImpl use_cases{
        [this] {
            return db_.CreateUnitOfWork();
        }
    };

    ui::View view{menu, use_cases, std::cin, std::cout};
    menu.Run();
}

}  // namespace bookypedia
