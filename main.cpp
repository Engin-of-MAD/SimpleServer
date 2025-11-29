// Пример использования:

#include "App/AsyncServer.h"
int main() {
    AsyncServer server("127.0.0.77", 8080);
    server.exec();  // Запускает сервер
    return 0;
}