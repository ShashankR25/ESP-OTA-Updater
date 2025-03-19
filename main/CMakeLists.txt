# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c rgb_led.c wifi_app2.c http_server2.c        # list the source files of this component
    INCLUDE_DIRS        # optional, add here public include directories
    #EMBED_FILES webpage/app.css webpage/app.js webpage/index.html webpage/favicon.ico webpage/jquery-3.3.1.min.js
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
