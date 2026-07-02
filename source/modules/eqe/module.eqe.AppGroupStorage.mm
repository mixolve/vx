#import <Foundation/Foundation.h>

#include "module.eqe.ProcessorSupport.h"

juce::File getEqeAppGroupContainerDirectory()
{
#if JUCE_IOS
    @autoreleasepool
    {
        auto* groupIdentifier = [NSString stringWithUTF8String: eqeAppGroupIdentifier];
        auto* containerUrl = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier: groupIdentifier];

        if (containerUrl == nil)
            return {};

        const char* path = containerUrl.path.UTF8String;

        if (path == nullptr)
            return {};

        auto directory = juce::File(juce::String::fromUTF8(path));
        directory.createDirectory();
        return directory.isDirectory() ? directory : juce::File();
    }
#else
    return {};
#endif
}
