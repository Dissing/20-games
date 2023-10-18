set dotenv-load

default:
  just --list

setup-build:
    mkdir -p $WORKSPACE/20-games/debug
    cmake -B $WORKSPACE/20-games/debug -S pong -GNinja \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
        -DCMAKE_BUILD_TYPE=Debug

    ln -s $WORKSPACE/20-games/debug/compile_commands.json .

build:
    ninja -C $WORKSPACE/20-games/debug

run:
    cd $WORKSPACE/20-games/debug/ && ./pong

format:
    find pong/src/ -iname *.h -o -iname *.c | xargs $FORMAT -style=file -i

b: build

r: run

br: build run

