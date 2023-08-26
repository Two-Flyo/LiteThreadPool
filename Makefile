# 编译器设置
CXX = g++
CXXFLAGS = -Ofast -Wall -Wextra -std=c++17
# 源文件和目标文件设置
SRCDIR = src
OBJDIR = build
BINDIR = bin

# 头文件和源文件列表
INCLUDES = -I$(SRCDIR)
SRCS = $(wildcard $(SRCDIR)/*.cpp)
OBJS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS))
TARGET = $(BINDIR)/myprogram

# 默认构建目标
all: $(TARGET)

# 目标文件的构建规则
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -pthread -o $@ $^

# 源文件到目标文件的编译规则
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# 清理生成的文件
clean:
	rm -f $(OBJDIR)/*.o $(TARGET)