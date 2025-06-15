# 模块名称
MODULE_NAME := $(STR_MODULE)
RL_MODULE_NAME := rl$(MODULE_NAME)
# 编译工具
MAKE_TOOL := $(MAKE_TOOL_CC)

# 编译路径
BUILD_DIR := $(shell pwd)/..
# 源文件路径
SRC_DIR := $(BUILD_DIR)/src
# 模块头文件路径
INCLUDE_DIR := $(BUILD_DIR)/include
# 编译所需头文件路径
MAKE_INCLUDE_DIR := $(PJ_INCLUDE_DIR)
MAKE_INCLUDE_DIR += $(INCLUDE_DIR)
# 生成目标文件路径
OBJ_DIR := $(BUILD_DIR)/object
# 生成库文件路径
LIB_DIR := $(BUILD_DIR)/lib

# 目标文件
TARGET := $(LIB_DIR)/lib$(MODULE_NAME).a
OBJ := $(OBJ_DIR)/$(RL_MODULE_NAME).o

# 创建目录
$(OBJ_DIR) $(LIB_DIR):
	mkdir -p $@

# 只支持 make MODULE_NAME
$(MODULE_NAME): $(TARGET)
	@echo "building $(RL_MODULE_NAME)..."

# 生成库文件,复制到目标目录(使用 shared 工具生成动态库)
$(TARGET): $(OBJ) | $(LIB_DIR)
	$(AR) rcs $@ $^
	cp $@ $(TARGET_LIB_A_DIR)
	if [ -d "$(INCLUDE_DIR)" ] && ls $(INCLUDE_DIR)/*.h; then \
		cp -f $(INCLUDE_DIR)/*.h $(CP_INCLUDE_DIR_RL)/; \
	fi

# 编译 C 文件（确保 .o 文件存放在 object 目录）
$(OBJ_DIR)/%.o: $(SRC_DIR)/$(RL_MODULE_NAME).c | $(OBJ_DIR)
	$(MAKE_TOOL) $(OPTIMIZE_CFLAGS) $(foreach dir, $(MAKE_INCLUDE_DIR), -I$(dir)) -c $< -o $@

# 清理 MODULE_NAME 相关文件
$(MODULE_NAME)_clean:
	@echo "cleaning $(RL_MODULE_NAME)..."
	rm -f $(OBJ_DIR)/* $(TARGET)

# 伪目标
.PHONY: $(MODULE_NAME) $(MODULE_NAME)_clean
