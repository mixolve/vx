#include "module.eqe.ProcessorSupport.h"

#include <algorithm>
#include <cmath>
#include <vector>

#if JUCE_IOS
static juce::File getDocumentsPresetStorageRootDirectory()
{
    auto directory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile(eqePresetStorageModuleFolder)
        .getChildFile(presetStorageRootFolder);
    directory.createDirectory();
    return directory;
}

static bool shouldCopyPresetFile(const juce::File& sourceFile, const juce::File& targetFile)
{
    if (! targetFile.existsAsFile())
        return true;

    if (sourceFile.hasIdenticalContentTo(targetFile))
        return false;

    return sourceFile.getLastModificationTime() >= targetFile.getLastModificationTime();
}

static void copyUpdatedPresetFiles(const juce::File& sourceDirectory, const juce::File& targetDirectory)
{
    if (! sourceDirectory.isDirectory())
        return;

    if (sourceDirectory.getFullPathName().equalsIgnoreCase(targetDirectory.getFullPathName()))
        return;

    targetDirectory.createDirectory();

    if (! targetDirectory.isDirectory())
        return;

    juce::Array<juce::File> sourceFiles;
    sourceDirectory.findChildFiles(sourceFiles, juce::File::findFiles, false, "*.xml");

    for (const auto& sourceFile : sourceFiles)
    {
        auto targetFile = targetDirectory.getChildFile(sourceFile.getFileName());

        if (shouldCopyPresetFile(sourceFile, targetFile))
        {
            if (targetFile.existsAsFile())
                targetFile.deleteFile();

            sourceFile.copyFileTo(targetFile);
        }
    }
}

static void syncDocumentsPresetsWithAppGroup(const juce::File& presetStorageRootDirectory)
{
    const auto documentsRootDirectory = getDocumentsPresetStorageRootDirectory();

    if (documentsRootDirectory.getFullPathName().equalsIgnoreCase(presetStorageRootDirectory.getFullPathName()))
        return;

    copyUpdatedPresetFiles(documentsRootDirectory, presetStorageRootDirectory);
    copyUpdatedPresetFiles(presetStorageRootDirectory, documentsRootDirectory);
}
#endif

static juce::File getPresetStorageRootDirectory()
{
    auto directory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

#if JUCE_IOS
    const auto appGroupContainer = getEqeAppGroupContainerDirectory();

    if (appGroupContainer.isDirectory())
        directory = appGroupContainer;
#else
    directory = directory.getChildFile(presetStorageVendorFolder)
                         .getChildFile(presetStorageProductFolder);
#endif

    directory = directory.getChildFile(eqePresetStorageModuleFolder)
                         .getChildFile(presetStorageRootFolder);
    directory.createDirectory();

    return directory;
}

static juce::File getFilterPresetsDirectory()
{
    auto directory = getPresetStorageRootDirectory();

#if JUCE_IOS
    syncDocumentsPresetsWithAppGroup(directory);
#endif

    directory.createDirectory();
    return directory;
}

static juce::String makePresetFileStem(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();
    auto fileStem = juce::File::createLegalFileName(trimmedName.toLowerCase()).trim();

    if (fileStem.isEmpty())
        fileStem = "preset";

    return fileStem;
}

static juce::File getPresetFileForName(const juce::File& directory, const juce::String& presetName)
{
    return directory.getChildFile(makePresetFileStem(presetName) + ".xml");
}

static bool containsFileNameIgnoreCase(const juce::StringArray& fileNames, const juce::String& fileName)
{
    for (const auto& candidate : fileNames)
    {
        if (candidate.equalsIgnoreCase(fileName))
            return true;
    }

    return false;
}

static bool writeXmlToFile(const juce::XmlElement& element, const juce::File& file)
{
    juce::TemporaryFile temporaryFile(file);

    if (auto outputStream = temporaryFile.getFile().createOutputStream())
    {
        outputStream->setPosition(0);
        outputStream->truncate();
        element.writeTo(*outputStream, {});
        outputStream->flush();
        return temporaryFile.overwriteTargetFileWithTemporary();
    }

    return false;
}

static std::unique_ptr<juce::XmlElement> loadXmlFile(const juce::File& file)
{
    if (! file.existsAsFile())
        return {};

    return juce::XmlDocument::parse(file);
}

static juce::String formatStoredFilterParameterId(const juce::String& parameterId)
{
    const auto trimmedId = parameterId.trim();

    const auto prefix = trimmedId.startsWithIgnoreCase("bell_") ? juce::String("bell_")
                                                                 : trimmedId.startsWithIgnoreCase("filter_") ? juce::String("filter_")
                                                                                                                : juce::String();

    if (prefix.isEmpty())
        return trimmedId;

    const auto remainder = trimmedId.substring(prefix.length());
    const auto separatorIndex = remainder.indexOfChar('_');

    if (separatorIndex <= 0)
        return trimmedId;

    const auto numberText = remainder.substring(0, separatorIndex);

    if (! numberText.containsOnly("0123456789"))
        return trimmedId;

    const auto suffix = remainder.substring(separatorIndex + 1);

    if (suffix.isEmpty())
        return trimmedId;

    if (suffix.equalsIgnoreCase("lrms"))
        return "filter_" + juce::String::formatted("%02d", numberText.getIntValue()) + "_place";

    if (suffix.equalsIgnoreCase("frequency"))
        return "filter_" + juce::String::formatted("%02d", numberText.getIntValue()) + "_freq";

    return "filter_" + juce::String::formatted("%02d", numberText.getIntValue()) + "_" + suffix;
}

static juce::String unformatStoredFilterParameterId(const juce::String& parameterId)
{
    const auto trimmedId = parameterId.trim();

    const auto prefix = trimmedId.startsWithIgnoreCase("bell_") ? juce::String("bell_")
                                                                 : trimmedId.startsWithIgnoreCase("filter_") ? juce::String("filter_")
                                                                                                                : juce::String();

    if (prefix.isEmpty())
        return trimmedId;

    const auto remainder = trimmedId.substring(prefix.length());
    const auto separatorIndex = remainder.indexOfChar('_');

    if (separatorIndex <= 0)
        return trimmedId;

    const auto numberText = remainder.substring(0, separatorIndex);

    if (! numberText.containsOnly("0123456789"))
        return trimmedId;

    const auto suffix = remainder.substring(separatorIndex + 1);

    if (suffix.isEmpty())
        return trimmedId;

    if (suffix.equalsIgnoreCase("place"))
        return "filter_" + juce::String(numberText.getIntValue()) + "_lrms";

    if (suffix.equalsIgnoreCase("freq"))
        return "filter_" + juce::String(numberText.getIntValue()) + "_frequency";

    return "filter_" + juce::String(numberText.getIntValue()) + "_" + suffix;
}

static int getStoredFilterParameterIndex(const juce::String& parameterId)
{
    const auto trimmedId = parameterId.trim();

    const auto prefix = trimmedId.startsWithIgnoreCase("bell_") ? juce::String("bell_")
                                                                 : trimmedId.startsWithIgnoreCase("filter_") ? juce::String("filter_")
                                                                                                               : juce::String();

    if (prefix.isEmpty())
        return -1;

    const auto remainder = trimmedId.substring(prefix.length());
    const auto separatorIndex = remainder.indexOfChar('_');

    if (separatorIndex <= 0)
        return -1;

    const auto numberText = remainder.substring(0, separatorIndex);

    if (! numberText.containsOnly("0123456789"))
        return -1;

    const auto normalizedSuffix = remainder.substring(separatorIndex + 1).trim().toLowerCase();

    if (normalizedSuffix.isEmpty())
        return -1;

    if (normalizedSuffix != "type"
        && normalizedSuffix != "ttss"
        && normalizedSuffix != "place"
        && normalizedSuffix != "slope"
        && normalizedSuffix != "freq"
        && normalizedSuffix != "bandwidth"
        && normalizedSuffix != "gain"
        && normalizedSuffix != "bypass")
    {
        return -1;
    }

    return numberText.getIntValue();
}

static int getStoredFilterParameterSortRank(const juce::String& parameterId)
{
    const auto trimmedId = parameterId.trim();

    const auto prefix = trimmedId.startsWithIgnoreCase("filter_") ? juce::String("filter_")
                                                                    : juce::String();

    if (prefix.isEmpty())
        return 100;

    const auto remainder = trimmedId.substring(prefix.length());
    const auto separatorIndex = remainder.indexOfChar('_');

    if (separatorIndex <= 0)
        return 100;

    const auto numberText = remainder.substring(0, separatorIndex);

    if (! numberText.containsOnly("0123456789"))
        return 100;

    const auto suffix = remainder.substring(separatorIndex + 1).trim().toLowerCase();

    if (suffix.isEmpty())
        return 100;

    const auto bandIndex = numberText.getIntValue() - 1;
    const auto parameterRank = suffix == "type" ? 0
        : suffix == "ttss" ? 1
        : suffix == "place" ? 2
        : suffix == "slope" ? 3
        : suffix == "freq" ? 4
        : suffix == "bandwidth" ? 5
        : suffix == "gain" ? 6
        : suffix == "bypass" ? 7
        : 8;

    return (bandIndex * 10) + parameterRank + 1;
}

static bool shouldFormatFilterParameterValueAsTwoDecimals(const juce::String& parameterId)
{
    const auto trimmedId = parameterId.trim().toLowerCase();
    return trimmedId.endsWith("_freq")
        || trimmedId.endsWith("_gain")
        || trimmedId.endsWith("_bandwidth");
}

static bool shouldFormatFilterParameterValueAsTwoDigits(const juce::String& parameterId)
{
    const auto trimmedId = parameterId.trim().toLowerCase();
    return trimmedId.endsWith("_type")
        || trimmedId.endsWith("_ttss")
        || trimmedId.endsWith("_place")
        || trimmedId.endsWith("_slope")
        || trimmedId.endsWith("_bypass");
}

static void formatStoredParameterValue(juce::XmlElement& element)
{
    const auto parameterId = element.getStringAttribute("id").trim();

    if (! shouldFormatFilterParameterValueAsTwoDecimals(parameterId))
    {
        if (! shouldFormatFilterParameterValueAsTwoDigits(parameterId))
            return;

        const auto value = static_cast<int>(std::lround(element.getDoubleAttribute("value", 0.0)));
        element.setAttribute("value", juce::String::formatted("%02d", value));
        return;
    }

    const auto value = element.getDoubleAttribute("value", 0.0);
    element.setAttribute("value", juce::String::formatted("%.2f", static_cast<double>(value)));
}

void rewriteFilterParameterIds(juce::XmlElement& element, const bool toStorageFormat)
{
    if (element.hasTagName("PARAM"))
    {
        const auto currentId = element.getStringAttribute("id").trim();
        const auto rewrittenId = toStorageFormat ? formatStoredFilterParameterId(currentId)
                                                 : unformatStoredFilterParameterId(currentId);

        if (rewrittenId.isNotEmpty() && rewrittenId != currentId)
            element.setAttribute("id", rewrittenId);
    }

    for (auto* child : element.getChildIterator())
        rewriteFilterParameterIds(*child, toStorageFormat);
}

static juce::StringArray collectPresetFileNames(const juce::XmlElement& rootElement,
                                                 const juce::File& directory,
                                                 const juce::String& presetElementTag)
{
    juce::StringArray fileNames;

    for (auto* child : rootElement.getChildIterator())
    {
        if (! child->hasTagName(presetElementTag))
            continue;

        const auto presetName = child->getStringAttribute("name").trim();

        if (presetName.isNotEmpty())
            fileNames.addIfNotAlreadyThere(getPresetFileForName(directory, presetName).getFileName());
    }

    return fileNames;
}

static bool writePresetCollectionToDirectory(const juce::File& directory,
                                             const juce::XmlElement& rootElement,
                                             const juce::String& presetElementTag)
{
    directory.createDirectory();

    const auto expectedFileNames = collectPresetFileNames(rootElement, directory, presetElementTag);

    for (auto* child : rootElement.getChildIterator())
    {
        if (! child->hasTagName(presetElementTag))
            continue;

        const auto presetName = child->getStringAttribute("name").trim();

        if (presetName.isEmpty())
            continue;

        auto presetCopy = std::make_unique<juce::XmlElement>(*child);
        presetCopy->removeAttribute(EqeModuleProcessor::filterPresetLastSelectedStateKey);
        presetCopy->removeAttribute(EqeModuleProcessor::filterPresetDefaultSelectedStateKey);
        presetCopy->removeAttribute("last_selected");
        presetCopy->removeAttribute("default_selected");

        if (! writeXmlToFile(*presetCopy, getPresetFileForName(directory, presetName)))
            return false;
    }

    juce::Array<juce::File> existingFiles;
    directory.findChildFiles(existingFiles, juce::File::findFiles, false, "*.xml");

    for (const auto& existingFile : existingFiles)
    {
        if (! containsFileNameIgnoreCase(expectedFileNames, existingFile.getFileName()))
            existingFile.deleteFile();
    }

    return true;
}

static std::unique_ptr<juce::XmlElement> loadPresetCollectionFromDirectory(const juce::File& directory,
                                                                           const juce::String& rootTag,
                                                                           const juce::String& presetElementTag)
{
    juce::Array<juce::File> presetFiles;
    directory.findChildFiles(presetFiles, juce::File::findFiles, false, "*.xml");

    if (presetFiles.isEmpty())
        return {};

    auto collection = std::make_unique<juce::XmlElement>(rootTag);

    for (const auto& presetFile : presetFiles)
    {
        auto presetXml = loadXmlFile(presetFile);

        if (presetXml == nullptr || ! presetXml->hasTagName(presetElementTag))
            continue;

        const auto presetName = presetXml->getStringAttribute("name").trim();

        if (presetName.isEmpty())
            continue;

        if (auto* existingPreset = findPresetElement(*collection, presetName))
            collection->removeChildElement(existingPreset, true);

        collection->addChildElement(presetXml.release());
    }

    if (collection->getNumChildElements() == 0)
        return {};

    return collection;
}

juce::XmlElement* findPresetElement(juce::XmlElement& rootElement, const juce::String& presetName)
{
    for (auto* child : rootElement.getChildIterator())
    {
        if (child->hasTagName(presetTag) && child->getStringAttribute("name").equalsIgnoreCase(presetName))
            return child;
    }

    return nullptr;
}

std::unique_ptr<juce::XmlElement> loadFilterPresetsXml()
{
    return loadPresetCollectionFromDirectory(getFilterPresetsDirectory(),
                                             filterPresetsRootTag,
                                             presetTag);
}

std::unique_ptr<juce::XmlElement> createEmptyFilterPresetsXml()
{
    return std::make_unique<juce::XmlElement>(filterPresetsRootTag);
}

bool writeFilterPresetsXml(const juce::XmlElement& rootElement)
{
    const auto filterPresetsDirectory = getFilterPresetsDirectory();

    if (! writePresetCollectionToDirectory(filterPresetsDirectory, rootElement, presetTag))
        return false;

    return true;
}

std::unique_ptr<juce::XmlElement> createSerializableStateXml(const EqeModuleProcessor& processor)
{
    return createSerializableStateXml(const_cast<EqeModuleProcessor&>(processor).getValueTreeState(),
                                      processor.getActiveBellCount());
}

std::unique_ptr<juce::XmlElement> createSerializableStateXml(juce::AudioProcessorValueTreeState& parameters,
                                                             const int activeBellCount)
{
    auto state = parameters.copyState();
    state.setProperty(EqeModuleProcessor::activeBellCountStateKey, activeBellCount, nullptr);
    state.removeProperty(EqeModuleProcessor::filterPresetLastSelectedStateKey, nullptr);
    state.removeProperty(EqeModuleProcessor::filterPresetDefaultSelectedStateKey, nullptr);

    auto stateXml = state.createXml();

    if (stateXml == nullptr)
        return {};

    struct ChildElementWithSortKey
    {
        int sortRank = 0;
        std::unique_ptr<juce::XmlElement> element;
    };

    std::vector<ChildElementWithSortKey> childElements;

    for (auto* child : stateXml->getChildIterator())
    {
        auto childCopy = std::make_unique<juce::XmlElement>(*child);

        rewriteFilterParameterIds(*childCopy, true);
        formatStoredParameterValue(*childCopy);

        const auto parameterId = childCopy->getStringAttribute("id").trim();
        const auto filterIndex = getStoredFilterParameterIndex(parameterId);

        if (parameterId.startsWithIgnoreCase("eqe_"))
            continue;

        if (filterIndex < 0 && parameterId.startsWithIgnoreCase("filter_"))
            continue;

        if (filterIndex > activeBellCount)
            continue;

        childElements.push_back({ getStoredFilterParameterSortRank(parameterId), std::move(childCopy) });
    }

    std::sort(childElements.begin(),
              childElements.end(),
              [] (const ChildElementWithSortKey& left, const ChildElementWithSortKey& right)
              {
                  return left.sortRank < right.sortRank;
              });

    auto storageState = std::make_unique<juce::XmlElement>(stateXml->getTagName());
    storageState->setAttribute(EqeModuleProcessor::activeBellCountStateKey, activeBellCount);

    for (auto& childElement : childElements)
        storageState->addChildElement(childElement.element.release());

    return storageState;
}
