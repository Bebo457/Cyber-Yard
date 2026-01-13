#!/bin/bash
# =============================================================================
# Cyber-Yard Docker Runner
# =============================================================================
# Automatycznie instaluje zależności, buduje obraz i uruchamia grę graficznie.
# Obsługuje GPU Nvidia/AMD/Intel.
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
IMAGE_NAME="cyber-yard"

# =============================================================================
# Funkcje pomocnicze
# =============================================================================

print_info() {
    echo -e "\033[1;34m[INFO]\033[0m $1"
}

print_success() {
    echo -e "\033[1;32m[OK]\033[0m $1"
}

print_warning() {
    echo -e "\033[1;33m[WARNING]\033[0m $1"
}

print_error() {
    echo -e "\033[1;31m[ERROR]\033[0m $1"
}

install_if_missing() {
    for pkg in "$@"; do
        if ! dpkg -s "$pkg" &>/dev/null; then
            print_info "Instaluję $pkg..."
            sudo apt-get install -y "$pkg"
        fi
    done
}

# =============================================================================
# Sprawdzenie DISPLAY
# =============================================================================

if [ -z "$DISPLAY" ]; then
    print_error "Zmienna DISPLAY nie jest ustawiona. Uruchom skrypt w środowisku graficznym."
    exit 1
fi

# =============================================================================
# Aktualizacja systemu i podstawowe narzędzia
# =============================================================================

print_info "Sprawdzam zależności systemowe..."
sudo apt-get update || true
install_if_missing pciutils x11-xserver-utils curl ca-certificates gnupg git

# =============================================================================
# Instalacja Docker (jeśli brak)
# =============================================================================

if ! command -v docker &>/dev/null; then
    print_info "Docker nie znaleziony. Instaluję..."
    
    # Dodaj klucz GPG Docker
    sudo install -m 0755 -d /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
    sudo chmod a+r /etc/apt/keyrings/docker.gpg
    
    # Dodaj repozytorium Docker
    echo \
      "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
      https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
      sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
    
    sudo apt-get update
    sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
    
    # Dodaj użytkownika do grupy docker
    if ! getent group docker > /dev/null; then
        sudo groupadd docker
    fi
    sudo usermod -aG docker "$USER"
    
    print_success "Docker zainstalowany."
    print_warning "Może być konieczne wylogowanie i ponowne zalogowanie, lub uruchomienie 'newgrp docker'."
fi

# =============================================================================
# Konfiguracja X11
# =============================================================================

print_info "Konfiguruję przekierowanie X11..."

xhost +local:docker 2>/dev/null || true

XAUTH_FILE=$(mktemp /tmp/.docker.xauth.XXXXXX)
trap 'rm -f "$XAUTH_FILE"' EXIT

touch "$XAUTH_FILE"
xauth_list=$(xauth nlist "${DISPLAY}" 2>/dev/null || true)
if [ -n "$xauth_list" ]; then
    echo "$xauth_list" | sed -e 's/^..../ffff/' | xauth -f "$XAUTH_FILE" nmerge -
fi
chmod a+r "$XAUTH_FILE"

DOCKER_GUI_ARGS=""
DOCKER_GUI_ARGS+=" --env=DISPLAY=$DISPLAY"
DOCKER_GUI_ARGS+=" --env=QT_X11_NO_MITSHM=1"
DOCKER_GUI_ARGS+=" --volume=/tmp/.X11-unix:/tmp/.X11-unix:rw"
DOCKER_GUI_ARGS+=" --env=XAUTHORITY=$XAUTH_FILE"
DOCKER_GUI_ARGS+=" --volume=$XAUTH_FILE:$XAUTH_FILE:rw"

# =============================================================================
# Detekcja i konfiguracja GPU
# =============================================================================

DOCKER_GPU_ARGS=""

print_info "Wykrywam GPU..."

if lspci | grep -iq nvidia; then
    print_success "Wykryto kartę NVIDIA"
    
    # Instalacja nvidia-container-toolkit jeśli brak
    if ! dpkg -s nvidia-container-toolkit &>/dev/null; then
        print_info "Instaluję nvidia-container-toolkit..."
        curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | \
            sudo gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg
        curl -s -L https://nvidia.github.io/libnvidia-container/stable/deb/nvidia-container-toolkit.list | \
            sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
            sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list > /dev/null
        sudo apt-get update
        sudo apt-get install -y nvidia-container-toolkit
        sudo nvidia-ctk runtime configure --runtime=docker
        sudo systemctl restart docker
        print_success "nvidia-container-toolkit zainstalowany"
    fi
    
    DOCKER_GPU_ARGS="--runtime=nvidia --gpus all"
    DOCKER_GUI_ARGS+=" --env=__NV_PRIME_RENDER_OFFLOAD=1"
    DOCKER_GUI_ARGS+=" --env=__GLX_VENDOR_LIBRARY_NAME=nvidia"
    DOCKER_GUI_ARGS+=" --env=NVIDIA_VISIBLE_DEVICES=all"
    DOCKER_GUI_ARGS+=" --env=NVIDIA_DRIVER_CAPABILITIES=all"

elif lspci | grep -iqE 'vga.*amd|vga.*ati|radeon|vga.*intel|graphics.*intel'; then
    if lspci | grep -iqE 'vga.*amd|vga.*ati|radeon'; then
        print_success "Wykryto kartę AMD/ATI"
    else
        print_success "Wykryto zintegrowaną grafikę Intel"
    fi
    
    install_if_missing mesa-utils libgl1-mesa-dri
    DOCKER_GPU_ARGS="--device=/dev/dri --group-add video"

else
    print_warning "Nie wykryto obsługiwanego GPU. Próbuję użyć /dev/dri..."
    if [ -d "/dev/dri" ]; then
        DOCKER_GPU_ARGS="--device=/dev/dri --group-add video"
    fi
fi

# =============================================================================
# Budowanie obrazu Docker
# =============================================================================

print_info "Buduję obraz Docker (może to potrwać kilka minut)..."

cd "$PROJECT_ROOT"
sudo docker build -t "$IMAGE_NAME" -f docker/Dockerfile .

print_success "Obraz $IMAGE_NAME zbudowany"

# =============================================================================
# Uruchomienie kontenera
# =============================================================================

print_info "Uruchamiam grę Cyber-Yard..."

sudo docker run -it --rm \
    --name cyber-yard-game \
    --network host \
    $DOCKER_GUI_ARGS \
    $DOCKER_GPU_ARGS \
    "$IMAGE_NAME"

