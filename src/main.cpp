#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Window.H>
#include <GL/glew.h>
#include <bitset>
#include <cstdint>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define ASSERT(X)                                                              \
  if (!(X))                                                                    \
    std::cerr << "Assertion Error: " << #X << " in file " << __FILE__          \
              << " at line: " << __LINE__ << std::endl;

constexpr int32_t window_width_v{1280};
constexpr int32_t window_height_v{720};

constexpr int32_t gl_viewport_width_v{window_width_v - 480};
constexpr int32_t gl_viewport_height_v{window_height_v};

constexpr int32_t fl_viewport_start_x{gl_viewport_width_v};
constexpr int32_t fl_viewport_start_y{0};

namespace canvas {
float height{32.0f};
float width{32.0f};
float cell_size{4.0f};
float scale{2.0f};
float start_x{50.0f};
float start_y{50.0f};
std::vector<bool> pixels(width *height);
}; // namespace canvas

struct Normalized {
  float x{}, y{}, z{};
};

constexpr Normalized normalized(float x, float y) {
  float norm_x{}, norm_y{};

  constexpr float gl_viewport_width_half_v{gl_viewport_width_v / 2};
  constexpr float gl_viewport_height_half_v{gl_viewport_height_v / 2};

  if (x > gl_viewport_width_half_v) {
    norm_x = (x - gl_viewport_width_half_v) / gl_viewport_width_half_v;
  } else if (x < gl_viewport_width_half_v) {
    norm_x = -((gl_viewport_width_half_v - x) / gl_viewport_width_half_v);
  } else if (x == gl_viewport_width_half_v) {
    norm_x = 0.0f;
  }

  if (y > gl_viewport_height_half_v) {
    norm_y = -((y - gl_viewport_height_half_v) / gl_viewport_height_half_v);
  } else if (y < gl_viewport_height_half_v) {
    norm_y = (gl_viewport_height_half_v - y) / gl_viewport_height_half_v;
  } else if (y == gl_viewport_height_half_v) {
    norm_y = 0.0f;
  }
  return Normalized{.x = norm_x, .y = norm_y, .z = 0.0f};
}

class GlWindow : public Fl_Gl_Window {
public:
  GlWindow(int32_t x, int32_t y, int32_t w, int32_t h, const char *l = "")
      : Fl_Gl_Window{x, y, w, h, l} {
    mode(FL_OPENGL3 | FL_RGB8);
    glViewport(0, 0, pixel_w(), pixel_h());

    std::fill(canvas::pixels.begin(), canvas::pixels.end(), 0);
    generate_grid();
  }

  void draw() {
    /* clang-format off */
    static constexpr const char *vertex_shader_source_v{
      "#version 330 core\n"
      "layout (location = 0) in vec3 aPos;\n"
      "void main()\n"
      "{\n"
      "  gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0f);\n"
      "}\n\0"
    };
    static constexpr const char *fragment_shader_source_v{
      "#version 330 core\n"
      "out vec4 FragColor;\n"
      "void main()\n"
      "{\n"
      "  FragColor = vec4(0.3f, 0.3f, 0.3f, 1.0f);\n"
      "}\n\0"
    };
    /* clang-format on */

    int32_t vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source_v, nullptr);
    glCompileShader(vertex_shader);

    int32_t fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source_v, nullptr);
    glCompileShader(fragment_shader);

    m_shader_program = glCreateProgram();
    glAttachShader(m_shader_program, vertex_shader);
    glAttachShader(m_shader_program, fragment_shader);
    glLinkProgram(m_shader_program);

    glDeleteShader(fragment_shader);
    glDeleteShader(vertex_shader);

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * m_lines_coords.size(),
                 m_lines_coords.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void *)(0));

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &m_VAO1);
    glGenBuffers(1, &m_VBO1);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO1);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO1);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * m_rects_coords.size(),
                 m_rects_coords.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(uint32_t) * m_rects_indices.size(),
                 m_rects_indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void *)(0));

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_shader_program);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_LINES, 0, m_lines_coords.size());
    glBindVertexArray(0);

    glBindVertexArray(m_VAO1);
    glDrawElements(GL_TRIANGLES, m_rects_indices.size(), GL_UNSIGNED_INT, 0);

    // for (int32_t i{}; i < m_rects_indices.size(); ++i) {
    // std::cout << m_rects_indices.at(i) << ", ";
    // if ((i + 1) % 6 == 0)
    // std::cout << std::endl;
    // }
    //
    // std::cout << std::endl;
  }

  int32_t handle(int32_t event) {
    static bool first{true};
    if (first && event == FL_SHOW && shown()) {
      first = false;
      make_current();
      glewInit();
    }

    static int32_t initial_mouse_x{Fl::event_x()},
        initial_mouse_y{Fl::event_y()};

    static int32_t current_mouse_x{Fl::event_x()},
        current_mouse_y{Fl::event_y()};

    static bool mouse_middle_held{false};
    static bool mouse_left_held{false};
    static bool mouse_right_held{false};

    if (mouse_middle_held) {

      current_mouse_x = Fl::event_x();
      current_mouse_y = Fl::event_y();

      canvas::start_x += current_mouse_x - initial_mouse_x;
      canvas::start_y += current_mouse_y - initial_mouse_y;

      generate_grid();
      redraw();

      initial_mouse_x = Fl::event_x();
      initial_mouse_y = Fl::event_y();

    } else {
      initial_mouse_x = Fl::event_x();
      initial_mouse_y = Fl::event_y();
    }

    if (mouse_left_held) {
      current_mouse_x = Fl::event_x();
      current_mouse_y = Fl::event_y();
      if (current_mouse_x > canvas::start_x &&
          current_mouse_x < canvas::start_x + (canvas::width * canvas::scale *
                                               canvas::cell_size) &&
          current_mouse_y > canvas::start_y &&
          current_mouse_y < canvas::start_y + (canvas::height * canvas::scale *
                                               canvas::cell_size)) {
        const size_t index =
            static_cast<size_t>(((current_mouse_y - canvas::start_y) /
                                 (canvas::cell_size * canvas::scale))) *
                canvas::width +
            static_cast<size_t>(((current_mouse_x - canvas::start_x) /
                                 (canvas::cell_size * canvas::scale)));

        canvas::pixels.at(index) = 1;

        generate_grid();
        redraw();
      }
    }

    if (mouse_right_held) {
      current_mouse_x = Fl::event_x();
      current_mouse_y = Fl::event_y();
      if (current_mouse_x > canvas::start_x &&
          current_mouse_x < canvas::start_x + (canvas::width * canvas::scale *
                                               canvas::cell_size) &&
          current_mouse_y > canvas::start_y &&
          current_mouse_y < canvas::start_y + (canvas::height * canvas::scale *
                                               canvas::cell_size)) {
        const size_t index =
            static_cast<size_t>(((current_mouse_y - canvas::start_y) /
                                 (canvas::cell_size * canvas::scale))) *
                canvas::width +
            static_cast<size_t>(((current_mouse_x - canvas::start_x) /
                                 (canvas::cell_size * canvas::scale)));

        canvas::pixels.at(index) = 0;

        generate_grid();
        redraw();
      }
    }

    switch (event) {
    case FL_FOCUS:
    case FL_UNFOCUS:
      return 1;
    case FL_PUSH:
    case FL_RELEASE:
      mouse_middle_held = (Fl::event_buttons() & FL_BUTTON2);
      mouse_left_held = (Fl::event_buttons() & FL_BUTTON1);
      mouse_right_held = (Fl::event_buttons() & FL_BUTTON3);
      return 1;

    case FL_MOUSEWHEEL:
      canvas::scale =
          std::max(std::min(canvas::scale - Fl::event_dy() * 0.1f, 5.0f), 1.0f);
      generate_grid();
      redraw();
      return 1;
    }

    return Fl_Gl_Window::handle(event);
  }

  void generate_grid() {
    m_lines_coords.clear();
    m_rects_coords.clear();
    m_rects_indices.clear();
    uint32_t offset{};
    for (int32_t i{}; i < static_cast<int32_t>(canvas::height + 1); ++i) {
      for (int32_t j{}; j < static_cast<int32_t>(canvas::width + 1); ++j) {
        Normalized line_start{
            normalized(canvas::start_x + j * canvas::cell_size * canvas::scale,
                       canvas::start_y + 0)};
        Normalized line_end{
            normalized(canvas::start_x + j * canvas::cell_size * canvas::scale,
                       canvas::start_y +
                           canvas::height * canvas::cell_size * canvas::scale)};
        m_lines_coords.push_back(line_start.x);
        m_lines_coords.push_back(line_start.y);
        m_lines_coords.push_back(line_start.z);
        m_lines_coords.push_back(line_end.x);
        m_lines_coords.push_back(line_end.y);
        m_lines_coords.push_back(line_end.z);
      }
    }

    for (int32_t i{}; i < static_cast<int32_t>(canvas::height + 1); ++i) {
      for (int32_t j{}; j < static_cast<int32_t>(canvas::width + 1); ++j) {
        Normalized line_start{normalized(
            canvas::start_x,
            canvas::start_y + i * canvas::cell_size * canvas::scale)};
        Normalized line_end{normalized(
            canvas::start_x + canvas::width * canvas::cell_size * canvas::scale,
            canvas::start_y + i * canvas::cell_size * canvas::scale)};
        m_lines_coords.push_back(line_start.x);
        m_lines_coords.push_back(line_start.y);
        m_lines_coords.push_back(line_start.z);
        m_lines_coords.push_back(line_end.x);
        m_lines_coords.push_back(line_end.y);
        m_lines_coords.push_back(line_end.z);
      }
    }

#define PUSH(R)                                                                \
  do {                                                                         \
    m_rects_coords.push_back(R.x);                                             \
    m_rects_coords.push_back(R.y);                                             \
    m_rects_coords.push_back(R.z);                                             \
  } while (0);

    for (int32_t i{}; i < static_cast<int32_t>(canvas::height); ++i) {
      for (int32_t j{}; j < static_cast<int32_t>(canvas::width); ++j) {
        if (canvas::pixels.at(i * (canvas::width) + j)) {

          Normalized top_left{normalized(
              canvas::start_x + j * canvas::scale * canvas::cell_size,
              canvas::start_y + i * canvas::scale * canvas::cell_size)};
          Normalized top_right{normalized(
              canvas::start_x + j * canvas::cell_size * canvas::scale +
                  (canvas::cell_size * canvas::scale),
              canvas::start_y + i * canvas::scale * canvas::cell_size)};

          Normalized bottom_left{normalized(
              canvas::start_x + j * canvas::scale * canvas::cell_size,
              canvas::start_y + i * canvas::cell_size * canvas::scale +
                  (canvas::scale * canvas::cell_size))};

          Normalized bottom_right{normalized(
              canvas::start_x + j * canvas::cell_size * canvas::scale +
                  (canvas::cell_size * canvas::scale),
              canvas::start_y + i * canvas::cell_size * canvas::scale +
                  (canvas::cell_size * canvas::scale))};

          PUSH(top_right);
          PUSH(bottom_right);
          PUSH(bottom_left);
          PUSH(top_left);

          // std::cout << i * canvas::width + j << std::endl;
          //
          // std::cout << std::format("Top left: {}, {}", top_left.x,
          // top_left.y)
          // << std::endl;
          // std::cout << std::format("Top right: {}, {}", top_right.x,
          // top_right.y)
          // << std::endl;
          // std::cout << std::format("Bottom left: {}, {}", bottom_left.x,
          // bottom_left.y)
          // << std::endl;
          // std::cout << std::format("Bottom right: {}, {}", bottom_right.x,
          // bottom_right.y)
          // << std::endl;
          //
          m_rects_indices.push_back(offset);
          m_rects_indices.push_back(offset + 1);
          m_rects_indices.push_back(offset + 3);
          m_rects_indices.push_back(offset + 1);
          m_rects_indices.push_back(offset + 2);
          m_rects_indices.push_back(offset + 3);

          offset += 4;
        }
      }
    }
  }

#undef PUSH

  void resize(float width = 0.0f, float height = 0.0f) {
    if (width) {
      canvas::width = width;
    } else if (height) {
      canvas::height = height;
    }

    canvas::pixels.resize(canvas::width * canvas::height);
    generate_grid();
    redraw();
  }

private:
  uint32_t m_VBO{}, m_VAO{};
  uint32_t m_VBO1{}, m_VAO1{}, m_EBO{};
  int32_t m_shader_program{};
  std::vector<float> m_lines_coords{};
  std::vector<float> m_rects_coords{};
  std::vector<uint32_t> m_rects_indices{};
};

void clear_callback(Fl_Widget *w, void *data) {
  std::fill(canvas::pixels.begin(), canvas::pixels.end(), 0);
  GlWindow *window = static_cast<GlWindow *>(data);
  window->generate_grid();
  window->redraw();
}

void export_callback(Fl_Widget *w, void *data) {
  uint8_t byte{};
  size_t count{};
  std::stringstream buffer_data{};

  Fl_Text_Display *text_display = static_cast<Fl_Text_Display *>(data);

  auto reversed = [=](uint8_t n) -> uint8_t {
    uint8_t r{};
    int count{7};
    while (count >= 0) {
      r |= (((n >> count) & 1) << (7 - count));
      count--;
    }
    return r;
  };

  for (size_t i{}; i < canvas::height; ++i) {
    for (size_t j{}; j < canvas::width; ++j) {
      if (canvas::pixels.at(i * canvas::width + j)) {
        byte |= (1 << count);
      }
      count++;

      if (count == 8) {
        byte = reversed(byte);
        // std::cout << "0x" << std::hex << static_cast<uint32_t>(byte) << ", ";
        buffer_data << "0x" << std::hex << static_cast<uint32_t>(byte) << ", ";
        count = 0;
        byte = 0;
      }
    }
    buffer_data << "\n";
  }

  Fl_Text_Buffer *text_buffer = new Fl_Text_Buffer{};

  text_display->insert_position(0);
  text_buffer->replace(0, buffer_data.str().size() + 1,
                       buffer_data.str().c_str());
  text_display->buffer(text_buffer);

  Fl::copy(buffer_data.str().c_str(), buffer_data.str().size() + 1);
}

void canvas_resize_width(Fl_Widget *w, void *data) {
  Fl_Value_Slider *slider = static_cast<Fl_Value_Slider *>(w);
  std::cout << slider->value() << std::endl;
  GlWindow *window = static_cast<GlWindow *>(data);
  window->resize(slider->value(), 0.0f);
}

void canvas_resize_height(Fl_Widget *w, void *data) {
  Fl_Value_Slider *slider = static_cast<Fl_Value_Slider *>(w);
  std::cout << slider->value() << std::endl;
  GlWindow *window = static_cast<GlWindow *>(data);
  window->resize(0.0f, slider->value());
}

int main(int argc, char **argv) {

  Fl_Window *window = new Fl_Window{100, 100, window_width_v, window_height_v,
                                    "Oled Bitmap Generator"};
  GlWindow *gl_window =
      new GlWindow{0, 0, gl_viewport_width_v, gl_viewport_height_v};

  gl_window->end();

  Fl_Button *clear_button = new Fl_Button{
      fl_viewport_start_x + 50, fl_viewport_start_y + 10, 150, 50, "Clear"};
  clear_button->callback(clear_callback, gl_window);

  Fl_Text_Display *output_text = new Fl_Text_Display{
      fl_viewport_start_x + 100, fl_viewport_start_y + 300, 300, 300};

  Fl_Button *export_button = new Fl_Button{
      fl_viewport_start_x + 300, fl_viewport_start_y + 10, 150, 50, "Export"};
  export_button->callback(export_callback, output_text);

  Fl_Value_Slider *canvas_width_slider =
      new Fl_Value_Slider{fl_viewport_start_x + 100, fl_viewport_start_y + 100,
                          300, 25, "Canvas Width"};

  canvas_width_slider->type(FL_HORIZONTAL);

  canvas_width_slider->bounds(8.0f, 128.0f);
  canvas_width_slider->scrollvalue(32.0f, 8.0f, 8.0f, 128.0f);
  canvas_width_slider->step(8.0f);
  canvas_width_slider->callback(canvas_resize_width, gl_window);

  Fl_Value_Slider *canvas_height_slider =
      new Fl_Value_Slider{fl_viewport_start_x + 100, fl_viewport_start_y + 200,
                          300, 25, "Canvas Height"};

  canvas_height_slider->type(FL_HORIZONTAL);

  canvas_height_slider->bounds(8.0f, 128.0f);
  canvas_height_slider->scrollvalue(32.0f, 8.0f, 8.0f, 128.0f);
  canvas_height_slider->step(8.0f);
  canvas_height_slider->callback(canvas_resize_height, gl_window);

  window->end();

  window->show(argc, argv);
  gl_window->show();

  return Fl::run();
}
