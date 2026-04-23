
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1 || TARGET_OS_VISION == 1
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

#define IPLUG_AUVIEWCONTROLLER IPlugAUViewController_vFz10m
#define IPLUG_AUAUDIOUNIT IPlugAUAudioUnit_vFz10m
#import <Fz10mAU/IPlugAUViewController.h>
#import <Fz10mAU/IPlugAUAudioUnit.h>

//! Project version number for Fz10mAU.
FOUNDATION_EXPORT double Fz10mAUVersionNumber;

//! Project version string for Fz10mAU.
FOUNDATION_EXPORT const unsigned char Fz10mAUVersionString[];

@class IPlugAUViewController_vFz10m;
