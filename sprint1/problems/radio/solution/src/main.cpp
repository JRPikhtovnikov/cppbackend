#include "audio.h"
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

const size_t MAX_FRAMES = 65000;
const unsigned int SAMPLE_RATE = 44100;

// Функция сервера - принимает аудио и воспроизводит
void StartServer(uint16_t port) {
    try {
        io_service io_service;
        udp::socket socket(io_service, udp::endpoint(udp::v4(), port));
        
        cout << "Сервер запущен на порту " << port << endl;
        cout << "Ожидание аудиопотока..." << endl;
        
        // Инициализация аудио-плеера
        ma_format format = ma_format_u8;
        int channels = 1;
        Player player(format, channels, SAMPLE_RATE);
        
        vector<char> receive_buffer(MAX_FRAMES * 4); // Буфер с запасом
        udp::endpoint remote_endpoint;
        
        while (true) {
            // Получаем датаграмму
            boost::system::error_code error;
            size_t received_bytes = socket.receive_from(
                buffer(receive_buffer), remote_endpoint, 0, error);
            
            if (error && error != error::message_size) {
                cerr << "Ошибка приема: " << error.message() << endl;
                continue;
            }
            
            cout << "Получено " << received_bytes << " байт от "
                 << remote_endpoint.address().to_string() << endl;
            
            // Проверяем, что данные получены
            if (received_bytes > 0) {
                // Вычисляем количество фреймов
                size_t frame_size = player.GetFrameSize();
                size_t frame_count = received_bytes / frame_size;
                
                if (frame_count > 0) {
                    // Воспроизводим звук
                    // Время воспроизведения в миллисекундах
                    int play_duration_ms = static_cast<int>((frame_count * 1000) / SAMPLE_RATE);
                    
                    vector<char> audio_data(receive_buffer.begin(), 
                                           receive_buffer.begin() + received_bytes);
                    
                    player.PlayBuffer(audio_data, frame_count, play_duration_ms);
                }
            }
        }
    } catch (const std::exception& e) {
        cerr << "Ошибка сервера: " << e.what() << endl;
    }
}

// Функция клиента - записывает и отправляет аудио
void StartClient(uint16_t port) {
    try {
        io_service io_service;
        udp::socket socket(io_service, udp::v4());
        socket.open(udp::v4());
        
        // Инициализация аудио-рекордера
        ma_format format = ma_format_u8;
        int channels = 1;
        Recorder recorder(format, channels, SAMPLE_RATE);
        
        cout << "Клиент запущен. Используйте порт " << port << " для отправки" << endl;
        
        while (true) {
            string server_ip;
            cout << "\nВведите IP-адрес сервера для отправки аудио (или 'exit' для выхода): ";
            getline(cin, server_ip);
            
            if (server_ip == "exit" || server_ip == "quit") {
                break;
            }
            
            try {
                // Создаем endpoint сервера
                udp::endpoint server_endpoint(address::from_string(server_ip), port);
                
                cout << "Нажмите ENTER для начала записи аудиосообщения...";
                cin.ignore(); // Ожидаем ENTER
                
                cout << "Запись... (нажмите Ctrl+C в консоли для остановки)" << endl;
                
                // Записываем аудио
                // Время записи в миллисекундах (достаточно большое, но ограничим по фреймам)
                int record_duration_ms = 5000; // 5 секунд максимум
                auto audio_data = recorder.Record(MAX_FRAMES, record_duration_ms);
                size_t frame_count = audio_data.second;
                
                if (frame_count > 0) {
                    // Вычисляем количество байт для отправки
                    size_t frame_size = recorder.GetFrameSize();
                    size_t bytes_to_send = frame_count * frame_size;
                    
                    cout << "Записано " << frame_count << " фреймов (" 
                         << bytes_to_send << " байт)" << endl;
                    
                    // Отправляем данные
                    socket.send_to(buffer(audio_data.first.data(), bytes_to_send), 
                                  server_endpoint);
                    
                    cout << "Аудиосообщение отправлено на " << server_ip << endl;
                } else {
                    cout << "Не удалось записать аудио" << endl;
                }
                
            } catch (const boost::system::system_error& e) {
                cerr << "Ошибка сети: " << e.what() << endl;
            } catch (const std::exception& e) {
                cerr << "Ошибка: " << e.what() << endl;
            }
        }
        
    } catch (const std::exception& e) {
        cerr << "Ошибка клиента: " << e.what() << endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Использование: " << argv[0] << " <client|server> <port>" << endl;
        return 1;
    }
    
    string mode = argv[1];
    uint16_t port = static_cast<uint16_t>(stoi(argv[2]));
    
    if (mode == "server") {
        StartServer(port);
    } else if (mode == "client") {
        StartClient(port);
    } else {
        cerr << "Неверный режим. Используйте 'client' или 'server'" << endl;
        return 1;
    }
    
    return 0;
}