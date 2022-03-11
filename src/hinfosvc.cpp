#include <stdio.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <iterator>

int server_socket_fd = 0;
int active_socket_fd = 0;

const std::string empty_string = std::string();

typedef enum return_codes
{
  NO_ERROR,
  ARGUMENT_ERROR,
  SERVER_SOCKET_SETUP_ERROR,
} ReturnCodes;

/**
 * @brief Callback for cleaning sockets on exit
 */
void cleanup()
{
  if (active_socket_fd != 0)
  {
    close(active_socket_fd);
    active_socket_fd = 0;
  }

  if (server_socket_fd != 0)
  {
    close(server_socket_fd);
    server_socket_fd = 0;
  }
}

/**
 * @brief Helper function to print help
 * 
 * @param programName 
 */
void printHelp(const char *programName)
{
  fprintf(stderr, "Usage: %s <port>\nDescription: Opens HTTP server on port passed by argument and respond to HTTP request with informations about host.\n\nPossible requests:\nhostname\tSend hostname of pc where server is running\ncpu-name\tSend name of CPU and on some platforms brief information about it\nload\t\tSend current load of CPU\n", programName);
}

/**
 * @brief Send http response to socket
 * 
 * @param sock socket where message will get send
 * @param status_string string with code and name of status
 * @param content content of result
 * @return true on send success
 * @return false on send failed
 */
bool socketRespond(int sock, const std::string &status_string, const std::string &content = empty_string)
{
  std::stringstream result_stream;

  result_stream << "HTTP/1.1 " << status_string << "\r\n";
  result_stream << "Connection: close\r\n";
  result_stream << "Content-Length: " << content.length() << "\r\n";
  result_stream << "Access-Control-Allow-Origin: *\r\n";
  result_stream << "Content-Type: text/plain\r\n\r\n";
  result_stream << content;

  std::string result = result_stream.str();

  return send(sock, result.c_str(), result.length(), 0) != -1;
}

/**
 * @brief Execute commandline command
 * 
 * @param cmd command string
 * @return std::string result of execution
 */
std::string exec(const char *cmd)
{
  std::array<char, 128> buffer;
  std::string result;

  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
    throw std::runtime_error("Failed to execute command");

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    result += buffer.data();

  return result;
}

/**
 * @brief Get value from vector and convert it to unsigned int
 * 
 * @param values vector of values
 * @param index index of value
 * @return uint64_t converted value
 */
uint64_t GetValue(std::vector<std::string>& values, size_t index)
{
  try
  {
    return std::stoll(values.at(index));
  }
  catch (const std::exception &e)
  {
    return 0;
  }
}

/**
 * @brief Fetch processor stats
 * 
 * @return std::pair<uint64_t,uint64_t> pair of curent stats, idle and total
 */
std::pair<uint64_t,uint64_t> getProcStat()
{
  std::string line;
  std::ifstream file("/proc/stat");
  if (file.bad())
    throw std::runtime_error("Failed opening file /proc/stat");

  std::getline(file, line);

  std::istringstream iss(line);
  std::vector<std::string> results(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());
  size_t number_of_results = results.size();
  if (number_of_results == 0)
    throw std::runtime_error("Failed to retrieve proc stat results");

  // fprintf(stderr, "%ld\n", number_of_results);

  uint64_t idle = GetValue(results, 4) + GetValue(results, 5);
  uint64_t non_idle = GetValue(results, 1) + GetValue(results, 2) + GetValue(results, 3) + GetValue(results, 6) + GetValue(results, 7) + GetValue(results, 8);
  uint64_t total = idle + non_idle;

  return { idle, total };
}

/**
 * @brief Handle host name request
 * 
 * @param sock socket where result will get send
 * @return true on result send success
 * @return false on result send failed
 */
bool handleHostname(int sock)
{
  std::ifstream file("/proc/sys/kernel/hostname");
  if (file.bad())
    throw std::runtime_error("Can't open /proc/sys/kernel/hostname");

  std::stringstream buffer;
  buffer << file.rdbuf();

  std::string hostname = buffer.str();
  return socketRespond(sock, "200 OK", hostname.substr(0, hostname.length() - 1));
}

/**
 * @brief Handle cpu name request
 * 
 * @param sock socket where result will get send
 * @return true on result send success
 * @return false on result send failed
 */
bool handleCPUName(int sock)
{
  std::string cpu_name = exec("cat /proc/cpuinfo | grep 'model name' | head -n1 | awk -F':' '{ print $2 }'");
  return socketRespond(sock, "200 OK", cpu_name.substr(1, cpu_name.length() - 2));
}

/**
 * @brief Handle cpu load request
 * 
 * @param sock socket where result will get send
 * @return true on result send success
 * @return false on result send failed
 */
bool handleCPULoad(int sock)
{
  auto [first_idle, first_total] = getProcStat();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  auto [second_idle, second_total] = getProcStat();

  uint64_t total_diff = second_total - first_total;
  uint64_t idle_diff = second_idle - first_idle;

  double result = (double)(total_diff - idle_diff) / (double)total_diff;

  std::stringstream output_stream;
  output_stream << (int)(result * 100) << "%";

  return socketRespond(sock, "200 OK", output_stream.str());
}

/**
 * @brief Setup and run HTTP server
 * 
 * @param port server listening port
 * @return int Returns error code, never exits when operating ordinary
 */
int run_server(uint32_t port)
{
  int opt = 1;
  struct sockaddr_in address;
  size_t address_len = sizeof(address);
  char buffer[4096] = {0};
  ssize_t received_data = 0;
  std::string final_message;

  if ((server_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == 0)
  {
    fprintf(stderr, "[ERROR] Failed to create server socket\n");
    return SERVER_SOCKET_SETUP_ERROR;
  }

  if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
  {
    fprintf(stderr, "[ERROR] Failed to set socket options\n");
    return SERVER_SOCKET_SETUP_ERROR;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
  {
    fprintf(stderr, "[ERROR] Failed to bind server socket to port %d\n", port);
    return SERVER_SOCKET_SETUP_ERROR;
  }

  if (listen(server_socket_fd, 3) < 0)
  {
    fprintf(stderr, "[ERROR] Failed to listen on port %d\n", port);
    return SERVER_SOCKET_SETUP_ERROR;
  }

  while ((active_socket_fd = accept(server_socket_fd, (struct sockaddr *)&address, (socklen_t *)&address_len)) >= 0)
  {
    while (true)
    {
      memset((void *)buffer, 0, 4096 * sizeof(char));

      received_data = read(active_socket_fd, buffer, 4096);
      if (received_data > 0)
        final_message.append(buffer, received_data);

      if (received_data < 4096)
        break;
    }

    // fprintf(stderr, "%s\n", final_message.c_str());

    try
    {
      if (final_message.substr(5, 8) == "hostname")
      {
        if (!handleHostname(active_socket_fd))
        {
          fprintf(stderr, "[ERROR] [ERROR] /hostname - Failed to send repond\n");
        }
      }
      else if (final_message.substr(5, 8) == "cpu-name")
      {
        if (!handleCPUName(active_socket_fd))
        {
          fprintf(stderr, "[ERROR] [ERROR] /cpu-name - Failed to send repond\n");
        }
      }
      else if (final_message.substr(5, 4) == "load")
      {
        if (!handleCPULoad(active_socket_fd))
        {
          fprintf(stderr, "[ERROR] [ERROR] /load - Failed to send repond\n");
        }
      }
      else
      {
        if (!socketRespond(active_socket_fd, "400 Bad Request", "Bad Request"))
        {
          fprintf(stderr, "[ERROR] 400 Bad Request - Failed to send repond\n");
        }
      }
    }
    catch (const std::exception &e)
    {
      fprintf(stderr, "[ERROR] %s\n", e.what());

      if (!socketRespond(active_socket_fd, "500 Internal Server Error", "Internal Server Error"))
      {
        fprintf(stderr, "[ERROR] 500 Internal Server Error - Failed to send repond\n");
      }
    }

    final_message.clear();

    close(active_socket_fd);
    active_socket_fd = 0;
  }

  return NO_ERROR;
}

/**
 * @brief Program entrypoint
 * 
 * Check if we have exactly 1 user defined argument, parse and check it and run server
 * 
 * @param argc number of arguments
 * @param argv list of arguments
 * @return int Returns error code, program never exits when operating ordinary
 */
int main(int argc, char **argv)
{
  // Check if we have exactly 2 arguments (1 user defined)
  if (argc != 2)
  {
    // If not print help
    printHelp(argv[0]);
    return ARGUMENT_ERROR;
  }

  // If user argument is help then print help
  if (strcmp(argv[1], "--help") == 0 ||
      strcmp(argv[1], "-h") == 0)
  {
    printHelp(argv[0]);
    return NO_ERROR;
  }

  // Set cleanup callback
  atexit(cleanup);

  // Extract port from argument
  char *rest;
  uint32_t port = strtoul(argv[1], &rest, 10);

  // Check if port is valid
  if (*rest || port > 0xFFFF)
  {
    fprintf(stderr, "[ERROR] '%s' is not valid port\n", argv[1]);
    printHelp(argv[0]);
    return ARGUMENT_ERROR;
  }

  return run_server(port);
}
