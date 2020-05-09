This is a cookiecutter template for anyone who wants to create an ESP32 application
who's primary purpose is to be a web server and to be interacted with via a REST
API.

Small additional functions/features are added to demo common functionality
so you can spend less time reading documentation and more time getting
your project working!

This project is not meant to serve many users concurrently, its primarily
meant to just be a simple, approachable way to add REST functionality
to simple DIY projects.

# Usage
To render this template into a working project directory, you need to use the
command-line utility `cookiecutter`. This can be installed via `pip3 install cookiecutter`.

```
cookiecutter -c v0.0.0 git@github.com:BrianPugh/cookiecutter-esp32-webserver.git
```

## Routes

Add new routes to `src/route/v1/*`. As you make backwards incompatible updates
to certain endpoints, you can create a new folder `v2` while maintaining older
routes.

Adding routes involves 2 parts:

1. Writing the handler function (see `src/route/v1/examples.c`)
2. Registering the handler with a route and command type in the `register_routes`
   function of `src/route.c`.

Thats it!

# Features

## WebServer
Many times you just want a web server to serve a REST API to control hardware.
This sets up code and gives you nearby simple examples to base your API of of.

## Git Repo
A small nicety is that this automaticall intializes your git repo upon
creation.

## OTA
This sets up the boiler plate to allow OTA over WiFi via 2 different methods:

1. Querying a URL for TODO upon startup 
2. Uploading over the endpoint `/api/v1/ota`

# Misc tips

If you are unfamiliar with working with C/esp-idf projects, this section is 
meant to help out simple oversights/issues.

## Undefined references
If everything compiles, but you are having undefined references at the end
during linking, its probably because your forgot to add the c-file to 
`src/CMakeLists.txt`.
