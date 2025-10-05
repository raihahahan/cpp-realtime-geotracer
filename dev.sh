#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if Docker is running
check_docker() {
    if ! docker info > /dev/null 2>&1; then
        print_error "Docker is not running. Please start Docker Desktop and try again."
        exit 1
    fi
}

# Function to build the development environment
build() {
    print_status "Building development environment..."
    check_docker
    docker compose build
    print_success "Development environment built successfully!"
}

# Function to start the development environment
start() {
    print_status "Starting development environment..."
    check_docker
    docker compose up -d
    print_success "Development environment started!"
    print_status "You can now run: ./dev.sh shell"
}


# Function to get a shell in the container
shell() {
    check_docker
    if ! docker ps | grep -q cs3103; then
        print_warning "Container is not running. Starting it first..."
        start
    fi
    print_status "Opening shell in development container..."
    docker exec -it cs3103 /bin/bash
}

# Function to build and run a specific exercise
run() {
    if [ -z "$1" ]; then
        print_error "Please specify an exercise to run (e.g., ./dev.sh run ex1)"
        exit 1
    fi
    
    check_docker
    if ! docker ps | grep -q cs3103-dev; then
        print_warning "Container is not running. Starting it first..."
        start
    fi
    
    print_status "Building and running $1..."
    docker exec -it cs3103-dev bash -c "cd /workspace && make $1 && ./$1"
}


# Function to show help
show_help() {
    echo "cs3103 Development Environment Manager"
    echo ""
    echo "Usage: ./dev.sh [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  build       - Build the development environment"
    echo "  start       - Start the development environment"
    echo "  shell       - Open a shell in the container"
    echo ""
    echo "Examples:"
    echo "  ./dev.sh build        # Build the environment"
    echo "  ./dev.sh start        # Start the environment"
    echo "  ./dev.sh shell        # Open shell in container"
}

# Main script logic
case "${1:-help}" in
    build)
        build
        ;;
    start)
        start
        ;;
    shell)
        shell
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        print_error "Unknown command: $1"
        echo ""
        show_help
        exit 1
        ;;
esac
