This repository contains plugins I've made for the [VCV Rack](https://vcvrack.com/)
virtual modular synthesizer. See the plugin folders for details on each plugin.

## Building

To compile a plugin, run `RACK_DIR=/path/to/Rack-SDK make` in the plugin root
directory. To automatically copy the plugin to your Rack Plugins folder, replace
`make` with `make install`. For further details, see the [plugin development
tutorial](https://vcvrack.com/manual/PluginDevelopmentTutorial.html#creating-the-template-plugin).
