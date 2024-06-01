#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <memory>
#include <random>
#include <string>
#include <vector>

struct Player {
    std::string      id;
    std::string      posX;
    std::string      oldX;
    std::string      posY;
    std::string      oldY;
    std::string      direction;
    std::vector<int> posXVec;
    std::vector<int> posYVec;
};

struct Game {
    std::string      width;
    std::string      height;
    std::string      myId;
    std::vector<int> mapX;
    std::vector<int> mapY;
    std::vector<int> mapXnext;
    std::vector<int> mapYnext;
};

struct GPNTron : std::enable_shared_from_this<GPNTron> {
public:
    GPNTron(boost::asio::io_context& ioc_) : ioc{ioc_}, resolver{ioc}, socket{ioc} {}
    void                     start() { resolve(); }
    std::vector<std::string> msgVec{};
    std::vector<Player>      playerVec{};
    Game                     game{};
    Player                   me{};
    bool                     playerKnown{false};

private:
    boost::asio::io_context&       ioc;
    std::string                    recvBuffer;
    std::string                    sendBuffer;
    boost::asio::ip::tcp::resolver resolver;
    boost::asio::ip::tcp::socket   socket;
    bool                           hasError{false};
    std::mt19937_64                gen{std::random_device{}()};

    void resolve() {
        resolver.async_resolve(
          "gpn-tron.duckdns.org",
          "4000",
          [self = shared_from_this()](
            boost::system::error_code const&             error,
            boost::asio::ip::tcp::resolver::results_type results) {
              if(!error) {
                  self->start_connect(results);
              } else {
                  fmt::print(stderr, "Error: {}\n", error.message());
                  self->hasError = true;
              }
          });
    }

    void start_connect(boost::asio::ip::tcp::resolver::results_type results) {
        boost::asio::async_connect(
          socket,
          results,
          [self = shared_from_this()](
            boost::system::error_code const&                            error,
            boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint) {
              if(!error) {
                  fmt::print("Connected to: {}\n", endpoint.address().to_string());
                  self->init();
              } else {
                  fmt::print(stderr, "Error: {}\n", error.message());
                  self->hasError = true;
              }
          });
    }

    void init() {
        write("join|TheNicks|1a2b3c4d5e6f7g8h\n");
        start_read();
    }

    void write(std::string_view msg) {
        fmt::print("Send: {:?}\n", msg);
        sendBuffer = msg;
        boost::asio::async_write(
          socket,
          boost::asio::buffer(sendBuffer, sendBuffer.size()),
          [self = shared_from_this()](boost::system::error_code error, std::size_t) {
              if(error) {
                  fmt::print("Error: {}\n", error.message());
                  self->hasError = true;
              }
          });
    }

    void start_read() {
        if(hasError) {
            return;
        }

        auto const oldSize = recvBuffer.size();
        recvBuffer.resize(oldSize + 1024);

        socket.async_read_some(
          boost::asio::buffer(recvBuffer.data() + oldSize, 1024),
          [oldSize,
           self = shared_from_this()](boost::system::error_code const& error, std::size_t length) {
              if(!error) {
                  self->recvBuffer.resize(oldSize + length);
                  self->parse();
              } else {
                  fmt::print(stderr, "Error: {}\n", error.message());
                  self->hasError = true;
              }
          });
    }

    void parse() {
        auto pos = std::find(recvBuffer.begin(), recvBuffer.end(), '\n');
        while(pos != recvBuffer.end()) {
            handle(std::string_view{recvBuffer.begin(), pos});
            recvBuffer.erase(recvBuffer.begin(), pos + 1);
            pos = std::find(recvBuffer.begin(), recvBuffer.end(), '\n');
        }

        start_read();
    }

    std::vector<std::string> parseServerMsg(std::string_view message) {
        std::vector<std::string> parsedVec{};
        std::string_view         newMessage{};
        auto                     charIt = std::find(message.begin(), message.end(), '|');
        std::string              cmd{message.begin(), charIt};
        parsedVec.push_back(cmd);
        while(charIt != message.end()) {
            message = message.substr(std::distance(message.begin(), charIt + 1));
            charIt  = std::find(message.begin(), message.end(), '|');
            cmd     = std::string{message.begin(), charIt};
            parsedVec.push_back(cmd);
        }
        return parsedVec;
    }

    std::string calculateMove(Game& gameCalc, Player& meCalc) {
        //check for nearest player and change direction into different direction
        // Player{id, posX, oldx, posY, oldY, direction}
        //
        // move{up,right,down,left}
        fmt::print("start calculating! {} {}\n", meCalc.posX, meCalc.posY);
        std::size_t posXInt = std::stoi(meCalc.posX);
        std::size_t posYInt = std::stoi(meCalc.posY);
        std::string move{meCalc.direction};

        fmt::print("next move {}\n", move);
        if(move == "up") {
            if(gameCalc.mapYnext[posYInt - 1] == 1) {
                move = "left";
                if(gameCalc.mapXnext[posXInt - 1] == 1) {
                    move = "right";
                }
                fmt::print("next move corrected {}\n", move);
            }
        } else if(move == "down") {
            if(gameCalc.mapYnext[posYInt + 1] == 1) {
                move = "left";
                if(gameCalc.mapXnext[posXInt - 1] == 1) {
                    move = "right";
                }
                fmt::print("next move corrected {}\n", move);
            }
        } else if(move == "right") {
            if(gameCalc.mapXnext[posXInt + 1] == 1) {
                move = "up";
                if(gameCalc.mapYnext[posYInt + 1] == 1) {
                    move = "down";
                }
                fmt::print("next move corrected {}\n", move);
            }
        } else if(move == "left") {
            if(gameCalc.mapXnext[posXInt - 1] == 1) {
                move = "down";
                if(gameCalc.mapYnext[posXInt - 1] == 1) {
                    move = "up";
                }
                fmt::print("next move corrected {}\n", move);
            }
        }

        if(move == "left") {
            fmt::print(
              "Pos me :{} Pos mapp: {} next Pos: {}\n",
              posXInt,
              gameCalc.mapX[posXInt],
              gameCalc.mapXnext[posXInt - 1]);
        } else if(move == "right") {
            fmt::print(
              "Pos me :{} Pos map: {} next Pos: {}\n",
              posXInt,
              gameCalc.mapX[posXInt],
              gameCalc.mapXnext[posXInt + 1]);
        } else if(move == "up") {
            fmt::print(
              "Pos me :{} Pos map: {} next Pos: {}\n",
              posYInt,
              gameCalc.mapY[posYInt],
              gameCalc.mapYnext[posYInt + 1]);
        } else if(move == "down") {
            fmt::print(
              "Pos me:{} Pos map: {} next Pos: {}\n",
              posYInt,
              gameCalc.mapY[posYInt],
              gameCalc.mapYnext[posYInt - 1]);
        }

        meCalc.direction = move;
        return move;
    }

    //takes player ID an Position and saves it into the map
    void addPosToMap(std::size_t playerId, std::size_t x, std::size_t y) {
        game.mapX[x] = playerId;
        game.mapX[y] = playerId;
    }

    void delPlayerFromMap(std::size_t playerId) {
        std::size_t mapXSize{game.mapX.size()};
        std::size_t mapYSize{game.mapY.size()};
        for(std::size_t i; i < mapXSize; ++i) {
            if(game.mapX[i] == playerId) {
                game.mapX[i] = 0;
            }
        }
        for(std::size_t i; i < mapYSize; ++i) {
            if(game.mapY[i] == playerId) {
                game.mapY[i] = 0;
            }
        }
    }

    std::string calculateDirection(Player const& player) {
        std::string direction{};
        if(player.posX > player.oldX) {
            direction = "right";
        } else if(player.posX < player.oldX) {
            direction = "left";
        } else if(player.posY > player.posY) {
            direction = "up";
        } else if(player.posY < player.posY) {
            direction = "down";
        } else {
            direction = "";
        }
        return direction;
    }

    void handle(std::string_view message) {
        msgVec = parseServerMsg(message);
        std::size_t mapXSize{};
        std::size_t mapYSize{};
        fmt::print("Got: {:?}\n", message);
        if(message.starts_with("error")) {
            hasError = true;
        }
        if(message.starts_with("game")) {
            //write("message|Time to die 1 move away!\n");
            //game|width|height|myId
            playerVec.clear();
            game.width  = msgVec[1];
            game.height = msgVec[2];
            game.myId   = msgVec[3];
            me.id       = msgVec[3];
            fmt::print("My Id:{} !\n", me.id);
            mapXSize = std::stoi(msgVec[1]);
            mapYSize = std::stoi(msgVec[2]);
            game.mapX.resize(mapXSize);
            game.mapY.resize(mapYSize);
            for(std::size_t i = 0; i < mapXSize; ++i) {
                game.mapX[i] = 0;
            }
            for(std::size_t i = 0; i < mapYSize; ++i) {
                game.mapY[i] = 0;
            }
            game.mapXnext.resize(mapXSize);
            game.mapYnext.resize(mapYSize);
            for(std::size_t i = 0; i < mapXSize; ++i) {
                game.mapXnext[i] = 0;
            }
            for(std::size_t i = 0; i < mapYSize; ++i) {
                game.mapYnext[i] = 0;
            }
            fmt::print("Game Startup fin!");
        }
        if(message.starts_with("pos")) {
            //pos|id|x|y
            if(me.id == msgVec[1]) {
                me.posX = msgVec[2];
                me.posY = msgVec[3];
                me.direction = calculateDirection(me);
                if(me.direction == "") {
                    me.direction = "left";
                }
            }
            for(auto& player : playerVec) {
                if(player.id == msgVec[1]) {
                    playerKnown                          = true;
                    player.oldX                          = player.posX;
                    player.oldY                          = player.posY;
                    player.posX                          = msgVec[2];
                    player.posY                          = msgVec[3];
                    player.direction                     = calculateDirection(player);
                    game.mapX[std::stoi(msgVec[2])]      = 1;
                    game.mapY[std::stoi(msgVec[3])]      = 1;
                    player.posXVec[std::stoi(msgVec[2])] = 1;
                    player.posYVec[std::stoi(msgVec[3])] = 1;
                    if(player.direction == "up") {
                        game.mapYnext[std::stoi(msgVec[3]) + 1] = 1;

                    } else if(player.direction == "down") {
                        game.mapY[std::stoi(msgVec[3]) - 1] = 1;

                    } else if(player.direction == "left") {
                        game.mapXnext[std::stoi(msgVec[2]) - 1] = 1;

                    } else if(player.direction == "right") {
                        game.mapXnext[std::stoi(msgVec[2]) + 1] = 1;
                    }
                }
            }
            if(!playerKnown) {
                Player temp{};
                temp.id   = msgVec[1];
                temp.posX = msgVec[2];
                temp.posY = msgVec[3];
                temp.posXVec.resize(std::stoi(game.width));
                temp.posYVec.resize(std::stoi(game.height));
                for(int i = 0; i < game.mapX.size(); ++i) {
                    temp.posXVec[i] = 1;
                    game.mapX[i]    = 1;
                }
                for(int i = 0; i < game.mapY.size(); ++i) {
                    temp.posYVec[i] = 1;
                    game.mapY[i]    = 1;
                }
                playerVec.push_back(temp);
            }
            playerKnown = false;
        }

        if(message.starts_with("die")) {
            for(int i = 1; i < msgVec.size(); ++i) {
                delPlayerFromMap(std::stoi(msgVec[i]));
            }
        }
        if(message.starts_with("tick")) {
            std::array<std::string_view, 4> commands{
              "up",
              "down",
              "left",
              "right",
            };

            std::uniform_int_distribution<std::size_t> dist{0, commands.size() - 1};
            fmt::print("{} {}\n", me.posX, me.posY);
            write(fmt::format("move|{}\n", calculateMove(game, me)));
        }
    }
};

int main() {
    boost::asio::io_context ioc;
    auto                    game = std::make_shared<GPNTron>(ioc);

    game->start();
    ioc.run();
    return EXIT_SUCCESS;
}
