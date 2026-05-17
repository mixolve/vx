#import <Foundation/Foundation.h>

#include "ProcessorSupport.h"

juce::File getAppGroupContainerDirectory()
{
#if JUCE_IOS
    @autoreleasepool
    {
        auto* groupIdentifier = [NSString stringWithUTF8String:appGroupIdentifier];
        auto* containerUrl = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];

        if (containerUrl == nil)
            return {};

        const char* path = containerUrl.path.UTF8String;
        return path != nullptr ? juce::File(juce::String::fromUTF8(path)) : juce::File();
    }
#else
    return {};
#endif
}
