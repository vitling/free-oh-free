#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "Minimum OS X deployment version" FORCE)

project(FREE_OH_FREE_PLUGIN VERSION 0.0.1)

add_subdirectory(JUCE)

juce_add_plugin(FreeOhFreePlugin
        # ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/icon.png
        # ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/icon.png
        COMPANY_NAME vitling                
        IS_SYNTH TRUE                       
        NEEDS_MIDI_INPUT TRUE               
        NEEDS_MIDI_OUTPUT FALSE             
        IS_MIDI_EFFECT FALSE                
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE   
        COPY_PLUGIN_AFTER_BUILD TRUE        
        PLUGIN_MANUFACTURER_CODE Vitl               
        PLUGIN_CODE Free                            
        DESCRIPTION "TODO Add a good description here"
        VST3_CATEGORIES "Instrument Synth Stereo"
        AU_MAIN_TYPE "kAudioUnitType_MusicDevice"
        FORMATS VST3 AU Standalone              
        BUNDLE_ID "io.github.vitling.freeohfree"
        HARDENED_RUNTIME_ENABLED TRUE
        PRODUCT_NAME FreeOhFree)


juce_generate_juce_header(FreeOhFreePlugin)

target_sources(FreeOhFreePlugin PRIVATE
        FreeOhFreePlugin.cpp)

target_compile_definitions(FreeOhFreePlugin
        PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0

        # We don't have to display the splash screen since we're using JUCE
        # under the GPL
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FreeOhFreePlugin PRIVATE
        juce::juce_audio_utils)
