echo "=== Setup the Project ==="

# Activate Python virtual environment
source ~/zephyrproject/.venv/bin/activate

# Zephyr base
export ZEPHYR_BASE=~/zephyrproject/zephyr
export PARAMETERS="-b sahel_stm32h7b0 app -- -DBOARD_ROOT=$(pwd)"

echo "=== Environment ready ==="