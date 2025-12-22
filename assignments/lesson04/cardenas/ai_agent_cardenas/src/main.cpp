#include "AiAgent.h"
#include <iostream>

int main() {
    AiAgent agent;

    std::string err;
    if (!agent.loadConfig("../config.json", &err)) {
        std::cerr << "Config error: " << err << "\n";
        return 1;
    }

    std::cout << std::endl << agent.sayhello() << std::endl << std::endl;

    char new_prompt[1024];
    std::string history = "Это история прошлых сообшений: \n";
    std::string new_prompt_string;
    int flag = 0;
    int counter = 0;

    while(!std::cin.eof()) {

        if (!agent.loadPrompt("../prompt.json", &err)) {
            std::cerr << "Prompt error: " << err << "\n";
            return 1;
        }

        std::cout << "Пользователь: \n";
        std::cin.getline(new_prompt, 1024);
        counter++;

        history += "сообщение " + std::to_string(counter);
        history += ": ";
        history += new_prompt;
        history += "\n";

        agent.setPrompt(history);

        std::string request = "Запрос: ";
        agent.setPrompt(request);

        new_prompt_string = new_prompt;
        agent.setPrompt(new_prompt_string);

        if(std::cin.eof()) break;

        auto resp = agent.ask(&err);
        if (!resp) {
            flag = 1;
            break;
        }

        std::cout << "\nАгент: \n" << *resp << std::endl << std::endl;

    }

    if(std::cin.eof()) {
        std::cout << "\nАгент: \nДо встречи! \nСессия закончена\n";
        return 0;
    }

    if (flag = 1) {
        std::cerr << "Request failed: " << err << "\n";
        return 2;
    }

    return 0;
}
