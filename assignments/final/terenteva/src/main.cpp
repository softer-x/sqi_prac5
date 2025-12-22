#include "AiAgent.h"
#include "MusicAgent.h"
#include <iostream>
#include <string>

void printUsage() {
    std::cout << "Как пользоваться:\n"
              << " ./music_ai_agent --suggest - Советует подборку треков из Яндекс.Музыки, подходящих вашему текущему настроению\n"
              << " ./music_ai_agent --find - Может найти любой доступный в Яндекс.Музыке трек\n"
              << " ./music_ai_agent -help - Помощь в использовании\n"
              << " Дополнительно: --mode <hurated|local> - Выбрать режим работы\n";
}

std::string parseModeArg(int argc, char *argv[]) 
{
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            std::string mode = argv[i + 1];
            std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
            if (mode == "hurated" || mode == "local") {
                return mode;
            }
        }
    }
    return "";
}

int main(int argc, char *argv[])
{
    AiAgent aiagent;
    MusicAgent musicagent;

    std::cout << "Привет! Я - AI-ассистент для подбора музыки!\n";
    std::string err;
    if (!aiagent.loadConfig("config.json", &err)) {
        std::cerr << "Ai Config error: " << err << std::endl;
        return 1;
    }
    if (argc < 2) {
        printUsage();
        return 1;
    }
    std::string cliMode = parseModeArg(argc, argv);
    if (!cliMode.empty()) {
        if (!aiagent.setMode(cliMode, &err)) {
            std::cerr << "Ошибка установки режима: " << err << std::endl;
            return 1;
        }
    }
    std::string command = argv[1];
    if (command == "--suggest") {
        if (aiagent.getCurrentMode() == "local") {
            std::cout << "Режим локальной модели: AI сам предложит музыкальные рекомендации\n";
            std::cout << "Расскажи о своем настроении и состоянии, и я предложу подходящую музыку\n";
            std::cout << "> ";
            
            std::string mood;
            std::getline(std::cin, mood);
            
            if (mood.empty()) {
                std::cerr << "Ты не описал свое настроение:(\n";
                return 1;
            }
            
            std::cout << "\nРазмышляю о твоем настроении...\n\n";
            
            auto recommendations = aiagent.generateAndPrintRecommendations(mood, &err);
            if (!recommendations) {
                std::cerr << "Ошибка генерации рекомендаций: " << err << "\n";
                return 2;
            }
            
            std::cout << "\nПриятного прослушивания!\n";
            return 0;
        }
        if (!aiagent.loadPrompt("prompt.json", &err)) {
            std::cerr << "Ai Prompt error: " << err << std::endl;
            return 1;
        }
        if (!musicagent.loadConfig("config.json", &err)) {
            std::cerr << "Music Config error: " << err << std::endl;
            return 1;
        }

        std::cout << "Расскажи о своем настроении и состоянии, и я подберу подходящую музыку\n";
        std::cout << "> ";
        std::string mood;
        std::getline(std::cin, mood);
        if (mood.empty()) {
            std::cerr << "Ты не описал свое настроение:(\n";
            return 1;
        }

        std::cout << "Размышляю...\n";
        auto search = aiagent.generateMusic(mood, &err);
        if (!search) {
            std::cerr << "Ошибка генерации ответа: " << err << "\n";
            return 2;
        }

        auto recommendations = musicagent.getMusicRecommendation(*search, &err);
        if (!recommendations) {
            std::cerr << "Ошибка поиска:  " << err << "\n";
            return 3;
        }
        aiagent.printResult(*recommendations, mood, &err);
    } else if (command == "--find") {
        if (aiagent.getCurrentMode() == "local") {
            std::cout << "При использовании локальной модели невозможен поиск треков в базе, пожалуйста, используйте онлайн-модель" << std::endl;
            
            return 0;
        }
        if (!aiagent.loadPrompt("prompt.json", &err)) {
            std::cerr << "Ai Prompt error: " << err << std::endl;
            return 1;
        }
        if (!musicagent.loadConfig("config.json", &err)) {
            std::cerr << "Music Config error: " << err << std::endl;
            return 1;
        }   
        std::cout << "Какой бы трек ты хотел найти?\n";
        std::cout << "Пожалуйста напиши желаемый трек в следующем формате: <Название трека> - <Исполнитель>\n";
        std::cout << "Например: That's Life - Frank Sinatra\n";
        std::cout << ">";
        std::string search;
        std::getline(std::cin, search);
        if (search.empty()) {
            std::cerr << "Ты не ввел никакой трек:(\n";
            return 1;
        }
        auto recommendations = musicagent.findTrack(search, &err);
        if (!recommendations) {
            std::cerr << "Ошибка поиска: " << err << "\n";
            return 3;
        }
        aiagent.printResult(*recommendations, "", &err);
    } else if (command == "-help") {
        printUsage();
        return 1;
    } else {
        printUsage();
        return 1;
    }

    return 0;
}