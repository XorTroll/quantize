
NAME			:=	quantize
OUTPUT_DIR		:=	out
OUTPUT			:=	$(NAME)
INDEX_HTML		:=	source/index.html
INCLUDE			:=	-Iinclude
SOURCES			:=	$(shell find $(CURDIR)/source/ -type f -name "*.cpp")
ASSETS			:=	assets

IMGUI_DIR		:=	imgui
IMPLOT_DIR		:=	implot
EIGEN_DIR		:=	eigen

CXX				:=	emcc
CXX_FLAGS		:=	-std=c++20 -O2 -fexceptions

SOURCES			+=	$(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_widgets.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
SOURCES			+=	$(IMPLOT_DIR)/implot.cpp $(IMPLOT_DIR)/implot_items.cpp

LIBS			:=	-lGL
CXX_EMS_FLAGS	:=	-s USE_WEBGL2=1 -s USE_GLFW=3 -s FULL_ES3=1 -s TOTAL_MEMORY=256MB -s TOTAL_STACK=64MB -s WASM=1 -s RETAIN_COMPILER_SETTINGS -s ASSERTIONS -s EXPORTED_RUNTIME_METHODS=[ccall]

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	mkdir -p $(OUTPUT_DIR)
	$(CXX) $(SOURCES) $(CXX_FLAGS) -o $(OUTPUT_DIR)/$(OUTPUT).js $(LIBS) $(CXX_EMS_FLAGS) --preload-file $(ASSETS) -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(IMPLOT_DIR) -I$(EIGEN_DIR) $(INCLUDE)
	cp $(INDEX_HTML) $(OUTPUT_DIR)/index.html

clean:
	rm -rf $(OUTPUT_DIR)
