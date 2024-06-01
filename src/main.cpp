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
    std::string id;
    std::string posX;
    std::string oldX;
    std::string posY;
    std::string oldY;
    std::string direction;
    std::string oldDirection;
};

struct Game {
    std::string                   width;
    std::string                   height;
    std::string                   myId;
    std::vector<std::vector<int>> map;
    std::vector<std::vector<int>> mapNext;
    std::vector<int>              mapXnext;
    std::vector<int>              mapYnext;
};

struct GPNTron : std::enable_shared_from_this<GPNTron> {
public:
    GPNTron(boost::asio::io_context& ioc_) : ioc{ioc_}, resolver{ioc}, socket{ioc} {}
    void                               start() { resolve(); }
    std::vector<std::string>           msgVec{};
    std::vector<Player>                playerVec{};
    Game                               game{};
    Player                             me{};
    bool                               playerKnown{false};
    std::random_device                 rndDev;
    std::default_random_engine         engine{rndDev()};
    std::uniform_int_distribution<int> dist{0, 3};

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
        write("join|TheNicks_v2|1a2b3c4d5e6f7g8h\n");
        start_read();
    }

    void write(std::string_view msg) {
        fmt::print("Send: {:?}\n", msg);
        sendBuffer = msg;
        boost::asio::async_write(
          socket,
          boost::asio::buffer(sendBuffer, sendBuffer.size()),
          [self = shared_from_this()](boost::system::error_code error, int) {
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
          [oldSize, self = shared_from_this()](boost::system::error_code const& error, int length) {
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

    std::string checkMove() {
        //check for nearest player and change direction into different direction
        // Player{id, posX, oldx, posY, oldY, direction}
        //
        // move{up,right,down,left}
        fmt::print("start calculating! {} {}\n", me.posX, me.posY);
        std::string move{me.direction};
        int         posX     = std::stoi(me.posX);
        int         posY     = std::stoi(me.posY);
        int         nextPosX = std::stoi(me.posX);
        int         nextPosY = std::stoi(me.posY);
        int         id       = std::stoi(game.myId);

        fmt::print("next move {}\n", move);

        if(me.oldDirection == "right" && move == "left") {
            fmt::print("Nope would crash myself\n");
            move = "up";
        } else if(me.oldDirection == "left" && move == "right") {
            fmt::print("Nope would crash myself\n");
            move = "down";

        } else if(me.oldDirection == "up" && move == "down") {
            fmt::print("Nope would crash myself\n");
            move = "left";

        } else if(me.oldDirection == "down" && move == "up") {
            fmt::print("Nope would crash myself\n");
            move = "right";
        }

        if(move == "up") {
            if(game.mapNext[posY - 1][posX] != 0) {
                write("chat|we are a trap!\n");
                move     = "right";
                nextPosX = posX + 1;
                if(game.mapNext[posY][posX + 1] != 0 || me.oldDirection == "left") {
                    write("chat|Its a second trap!\n");
                    move     = "left";
                    nextPosX = posX - 1;
                }
            }
            nextPosY = posY - 1;
        }
        if(move == "left") {
            if(game.mapNext[posY][posX - 1] != 0) {
                write("chat|we are a trap!\n");
                move     = "up";
                nextPosY = posY - 1;
                if(game.mapNext[posY - 1][posX] != 0 || me.oldDirection == "down") {
                    write("chat|Its a second trap!\n");
                    move     = "down";
                    nextPosY = posY + 1;
                }
            }
            nextPosX = posX - 1;
        }
        if(move == "right") {
            if(game.mapNext[posY][posX + 1] != 0) {
                write("chat|we are a trap!\n");
                move     = "down";
                nextPosY = posY + 1;
                if(game.mapNext[posY + 1][posX] != 0 || me.oldDirection == "up") {
                    write("chat|Its a second trap!\n");
                    move     = "up";
                    nextPosY = posY - 1;
                }
            }
            nextPosX = posX + 1;
        }
        if(move == "down") {
            if(game.mapNext[posY + 1][posX] == 0) {
                write("chat|we are a trap!\n");
                move     = "right";
                nextPosX = posX + 1;
                if(game.mapNext[posY][posX + 1] == 0 || me.oldDirection == "left") {
                    write("chat|Its a second trap!\n");
                    move     = "left";
                    nextPosX = posX - 1;
                }
            }
            nextPosY = posY + 1;
        }
        addPosToMap(id, nextPosX, nextPosY, game.mapNext);
        me.direction = move;
        return move;
    }

    //takes player ID an Position and saves it into the map
    void addPosToMap(int playerId, int x, int y, std::vector<std::vector<int>>& map) {
        if(std::to_string(playerId) == game.myId) {
            fmt::print("add my self ({}, {})\n", x, y);
        }
        map[y][x] = playerId;
    }
    //Example (4 dead player): die|5|8|9|13
    void delPlayerFromMap(int playerId) {
        for(auto& xAxis : game.map) {
            for(auto& point : xAxis) {
                if(point == playerId) {
                    point = 0;
                }
            }
        }
    }

    void printMap(std::vector<std::vector<int>>& map) {
        for(auto const& xAxis : map) {
            for(auto const& point : xAxis) {
                if(point == 0) {
                    fmt::print("0");
                } else {
                    if(point != std::stoi(game.myId)) {
                        fmt::print("1");
                    } else {
                        fmt::print("X");
                    }
                }
            }
            fmt::print("\n");
        }
    }

    std::string calculateDirection(Player const& player, std::vector<std::vector<int>>& map) {
        std::string direction{};
        int         posX = std::stoi(player.posX);
        int         posY = std::stoi(player.posY);
        int         id   = std::stoi(player.id);
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
        int mapXSize{};
        int mapYSize{};
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
            game.map.clear();
            game.map.resize(mapXSize);
            for(int i = 0; i < game.map.size(); ++i) {
                game.map[i].resize(mapYSize);
            }
            for(int i = 0; i < mapXSize; ++i) {
                for(int idx = 0; idx < mapYSize; ++idx) {
                    game.map[i][idx] = 0;
                }
            }
            game.mapNext = game.map;
            fmt::print("Game Startup fin!");
        }
        if(message.starts_with("pos")) {
            //pos|id|x|y
            addPosToMap(std::stoi(msgVec[1]), std::stoi(msgVec[2]), std::stoi(msgVec[3]), game.map);
            addPosToMap(
              std::stoi(msgVec[1]),
              std::stoi(msgVec[2]),
              std::stoi(msgVec[3]),
              game.mapNext);
            if(me.id == msgVec[1]) {
                me.oldX = me.posX;
                me.oldY = me.posY;
                me.posX = msgVec[2];
                me.posY = msgVec[3];
                playerKnown      = true;
                //me.direction = calculateDirection(me, game.mapNext);
            }
            for(auto& player : playerVec) {
                if(player.id == msgVec[1]) {
                    playerKnown      = true;
                    player.oldX      = player.posX;
                    player.oldY      = player.posY;
                    player.posX      = msgVec[2];
                    player.posY      = msgVec[3];
                    player.direction = calculateDirection(player, game.mapNext);
                }
            }
            if(!playerKnown) {
                Player temp{};
                temp.id   = msgVec[1];
                temp.posX = msgVec[2];
                temp.posY = msgVec[3];
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
            fmt::print("Printing Map:\n");
            printMap(game.map);
            fmt::print("Printing Next Map:\n");
            printMap(game.mapNext);

            std::array<std::string_view, 4> commands{
              "up",
              "down",
              "left",
              "right",
            };
            int rndMove = dist(engine);
            while(commands[rndMove] == me.oldDirection) {
                rndMove = dist(engine);
            }
            me.direction    = commands[rndMove];
            me.oldDirection = me.direction;
            fmt::print("{} {} {}\n", me.posX, me.posY, me.direction);
            write(fmt::format("move|{}\n", checkMove()));
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
