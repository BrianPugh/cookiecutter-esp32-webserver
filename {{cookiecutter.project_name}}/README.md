
# {{cookiecutter.project_name}}

# Usage

First define the wifi network to connnect to via the configuration manager:

```
idf.py menuconfig
```

Configuration options are available under:

```
{{cookiecutter.project_name}} Configuration
```

Most notable, the options:

```
{{cookiecutter.project_name}} Configuration>WiFi SSID
{{cookiecutter.project_name}} Configuration>WiFi Password
```

Warning: these values will be contained in the built binary; be careful
of distributing firmware with baked-in secrets.
