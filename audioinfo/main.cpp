//
//  main.cpp
//  audioinfo
//
//  Created by David Balatero on 11/25/11.
//  Copyright 2011 MediaPiston. All rights reserved.
//

#include <iostream>
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AUGraph.h>
#import <CoreMIDI/CoreMIDI.h>

typedef struct MyMIDIPlayer {
  AUGraph     graph;
  AudioUnit   instrumentUnit;
} MyMIDIPlayer;

void setupAUGraph(MyMIDIPlayer *player) {
  NewAUGraph(&player->graph);
  
  AUNode outputNode, synthNode;
  
  // Initialize the output device.
  AudioComponentDescription desc;
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;
  
  AUGraphAddNode(player->graph, &desc, &outputNode);
  
  // The built-in default (softsynth) music device
  desc.componentType = kAudioUnitType_MusicDevice;
  desc.componentSubType = kAudioUnitSubType_DLSSynth;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  AUGraphAddNode(player->graph, &desc, &synthNode);
  
  // Connect the soft-synth to default output
  // (output 0 of synth to output 0 of outputNode)
  AUGraphConnectNodeInput(player->graph, synthNode, 0, outputNode, 0);
  
  // Open + initialize the graph.
  AUGraphOpen(player->graph);
  AUGraphInitialize(player->graph);
  
  // Read the node info.
  AUGraphNodeInfo(player->graph, synthNode, NULL, &(player->instrumentUnit));
}

// This is a callback function for when new MIDI devices are attached
// to the system.
//
// Creating a dummy callback that just prints.
static void MyMIDINotifyProc (const MIDINotification  *message, void *refCon) {
  printf("MIDI Notify, messageId=%d,", message->messageID);
}

// This is a callback that gets used for MIDI setup.
static void MyMIDIReadProc(const MIDIPacketList *packetList,
                           void *refCon,
                           void *connRefCon) {
  MyMIDIPlayer *player = (MyMIDIPlayer*) refCon;
  MIDIPacket *packet = (MIDIPacket *) packetList->packet;
  
  for (int i = 0; i < packetList->numPackets; i++) {
    Byte midiStatus = packet->data[0];
    Byte midiCommand = midiStatus >> 4;
    
    // NOTE OFF and NOTE ON events.
    // See CoreAudio book, page 165.
    if ((midiCommand == 0x08) || (midiCommand == 0x09)) {
      Byte note = packet->data[1] & 0x7F;
      Byte velocity = packet->data[2] & 0x7F;
      
      // Send the instrument unit a MIDI event.
      MusicDeviceMIDIEvent(player->instrumentUnit,
                           midiStatus,
                           note,
                           velocity,
                           0);
    }
    
    packet = MIDIPacketNext(packet);
  }
}

void setupMIDI(MyMIDIPlayer *player) {
  MIDIClientRef client;
  MIDIClientCreate(CFSTR("Core MIDI to Synth Demo"),
                   MyMIDINotifyProc,
                   player,
                   &client);
  
  // Create input port.
  MIDIPortRef inPort;
  MIDIInputPortCreate(client, CFSTR("Input port"),
                      MyMIDIReadProc, player, &inPort);
  
  // Hook up any MIDI devices that are connected to this
  // machine (USB keyboards, etc.).
  unsigned long sourceCount = MIDIGetNumberOfSources();
  printf ("%ld sources\n", sourceCount);
  for (int i = 0; i < sourceCount; ++i) {
    MIDIEndpointRef src = MIDIGetSource(i);
    CFStringRef endpointName = NULL;
    MIDIObjectGetStringProperty(src,
                                kMIDIPropertyName,
                                &endpointName);
    
    char endpointNameC[255];
    CFStringGetCString(endpointName, endpointNameC, 255,
                       kCFStringEncodingUTF8);
    
    printf("  source %d: %s\n", i, endpointNameC);
    MIDIPortConnectSource(inPort, src, NULL);
  }
}


// https://github.com/Henne/dosbox-svn/blob/9eee0f99e062773909509d41b35b032171222c82/src/gui/midi_coreaudio.h
int main (int argc, const char * argv[])
{
  MyMIDIPlayer player;
  
  printf("Initializing AU Graph...\n");
  setupAUGraph(&player);
  
  printf("Initializing Core MIDI and USB keyboards...\n");
  setupMIDI(&player);
  
  printf("Starting AU Graph!\n");
  
  // Start the graph!
  AUGraphStart(player.graph);
  
  printf("Sending some manual events!");
  
  // Send some manual MIDI events.
  int noteOn = 0x9 << 4;
  int noteOff = 0x8 << 4;
  int middleC = 0x3c;
  int velocity;
  
  for (int i = 0; i < 10; i++) {
    velocity = 64 + (i * 8);
    MusicDeviceMIDIEvent(player.instrumentUnit,
                         noteOn, // note on
                         middleC + (i * 2), // whole tone scale
                         velocity,
                         0);
    sleep(1);
  }
  
  printf("Done! Ctrl + C to quit, or play via your MIDI USB keyboard.");
  
  // Run until Ctrl +C.
  CFRunLoopRun();
}