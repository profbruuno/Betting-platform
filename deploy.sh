#!/bin/bash

echo "🚀 Deploying Football Betting Platform..."

# Install dependencies
echo "📦 Installing dependencies..."
sudo apt-get update
sudo apt-get install -y g++ sqlite3 libsqlite3-dev cmake

# Create project directory
mkdir -p football-betting
cd football-betting

# Save files
cat > index.html << 'EOF'
<!DOCTYPE html>
<html>
<!-- Paste the entire HTML content from above here -->
</html>
EOF

cat > server.cpp << 'EOF'
// Paste the entire server.cpp content from above here
EOF

cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.16)
project(FootballBetting)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)

# Find SQLite3
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

add_executable(betting_server server.cpp)

target_include_directories(betting_server PRIVATE ${SQLITE3_INCLUDE_DIRS})
target_link_libraries(betting_server ${SQLITE3_LIBRARIES} pthread)

if(WIN32)
    target_link_libraries(betting_server ws2_32 wsock32)
endif()
EOF

# Build the project
echo "🔨 Building server..."
mkdir -p build
cd build
cmake ..
make

echo "✅ Build complete!"
echo ""
echo "🎯 To run the server:"
echo "   ./betting_server"
echo ""
echo "🌐 Then open: http://localhost:8080"
echo ""
echo "💡 Default accounts:"
echo "   User: demo_user (Balance: $50)"
echo "   Admin: admin (Balance: $1000)"
