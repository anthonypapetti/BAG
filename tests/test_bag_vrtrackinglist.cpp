
#include "test_utils.h"
#include <bag_dataset.h>
#include <bag_metadata.h>
#include <bag_vrtrackinglist.h>

#include <catch2/catch_all.hpp>
#include <string>


using BAG::Dataset;
using BAG::VRTrackingList;

//NOTE .. Testing the methods that forward to std::vector<VRTrackingList::value_type> is not covered.

namespace {

const std::string kMetadataXML{R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<gmi:MI_Metadata xmlns:gmi="http://www.isotc211.org/2005/gmi"
    xmlns:bag="http://www.opennavsurf.org/schema/bag"
    xmlns:gco="http://www.isotc211.org/2005/gco"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opennavsurf.org/schema/bag http://www.opennavsurf.org/schema/bag/bag.xsd">
    <gmd:fileIdentifier>
        <gco:CharacterString>Unique Identifier</gco:CharacterString>
    </gmd:fileIdentifier>
    <gmd:language>
        <gmd:LanguageCode codeList="http://www.loc.gov/standards/iso639-2/" codeListValue="eng">eng</gmd:LanguageCode>
    </gmd:language>
    <gmd:characterSet>
        <gmd:MD_CharacterSetCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_CharacterSetCode" codeListValue="utf8">utf8</gmd:MD_CharacterSetCode>
    </gmd:characterSet>
    <gmd:hierarchyLevel>
        <gmd:MD_ScopeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_ScopeCode" codeListValue="dataset">dataset</gmd:MD_ScopeCode>
    </gmd:hierarchyLevel>
    <gmd:contact>
        <gmd:CI_ResponsibleParty>
            <gmd:individualName>
                <gco:CharacterString>Name of individual responsible for the BAG</gco:CharacterString>
            </gmd:individualName>
            <gmd:role>
                <gmd:CI_RoleCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#CI_RoleCode" codeListValue="pointOfContact">pointOfContact</gmd:CI_RoleCode>
            </gmd:role>
        </gmd:CI_ResponsibleParty>
    </gmd:contact>
    <gmd:dateStamp>
        <gco:Date>2012-01-27</gco:Date>
    </gmd:dateStamp>
    <gmd:metadataStandardName>
        <gco:CharacterString>ISO 19115</gco:CharacterString>
    </gmd:metadataStandardName>
    <gmd:metadataStandardVersion>
        <gco:CharacterString>2003/Cor.1:2006</gco:CharacterString>
    </gmd:metadataStandardVersion>
    <gmd:spatialRepresentationInfo>
        <gmd:MD_Georectified>
            <gmd:numberOfDimensions>
                <gco:Integer>2</gco:Integer>
            </gmd:numberOfDimensions>
            <gmd:axisDimensionProperties>
                <gmd:MD_Dimension>
                    <gmd:dimensionName>
                        <gmd:MD_DimensionNameTypeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_DimensionNameTypeCode" codeListValue="row">row</gmd:MD_DimensionNameTypeCode>
                    </gmd:dimensionName>
                    <gmd:dimensionSize>
                        <gco:Integer>100</gco:Integer>
                    </gmd:dimensionSize>
                    <gmd:resolution>
                        <gco:Measure uom="Metres">10</gco:Measure>
                    </gmd:resolution>
                </gmd:MD_Dimension>
            </gmd:axisDimensionProperties>
            <gmd:axisDimensionProperties>
                <gmd:MD_Dimension>
                    <gmd:dimensionName>
                        <gmd:MD_DimensionNameTypeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_DimensionNameTypeCode" codeListValue="column">column</gmd:MD_DimensionNameTypeCode>
                    </gmd:dimensionName>
                    <gmd:dimensionSize>
                        <gco:Integer>100</gco:Integer>
                    </gmd:dimensionSize>
                    <gmd:resolution>
                        <gco:Measure uom="Metres">10</gco:Measure>
                    </gmd:resolution>
                </gmd:MD_Dimension>
            </gmd:axisDimensionProperties>
            <gmd:cellGeometry>
                <gmd:MD_CellGeometryCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_CellGeometryCode" codeListValue="point">point</gmd:MD_CellGeometryCode>
            </gmd:cellGeometry>
            <gmd:transformationParameterAvailability>
                <gco:Boolean>1</gco:Boolean>
            </gmd:transformationParameterAvailability>
            <gmd:checkPointAvailability>
                <gco:Boolean>0</gco:Boolean>
            </gmd:checkPointAvailability>
            <gmd:cornerPoints>
                <gml:Point gml:id="id1">
                    <gml:coordinates cs="," decimal="." ts=" ">687910.000000,5554620.000000 691590.000000,5562100.000000</gml:coordinates>
                </gml:Point>
            </gmd:cornerPoints>
            <gmd:pointInPixel>
                <gmd:MD_PixelOrientationCode>center</gmd:MD_PixelOrientationCode>
            </gmd:pointInPixel>
        </gmd:MD_Georectified>
    </gmd:spatialRepresentationInfo>
    <gmd:referenceSystemInfo>
        <gmd:MD_ReferenceSystem>
            <gmd:referenceSystemIdentifier>
                <gmd:RS_Identifier>
                    <gmd:code>
                        <gco:CharacterString>PROJCS["UTM-19N-Nad83",
    GEOGCS["unnamed",
        DATUM["North_American_Datum_1983",
            SPHEROID["North_American_Datum_1983",6378137,298.2572201434276],
            TOWGS84[0,0,0,0,0,0,0]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        EXTENSION["Scaler","0,0,0,0.02,0.02,0.001"],
        EXTENSION["Source","CARIS"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-69],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1]]</gco:CharacterString>
                    </gmd:code>
                    <gmd:codeSpace>
                        <gco:CharacterString>WKT</gco:CharacterString>
                    </gmd:codeSpace>
                </gmd:RS_Identifier>
            </gmd:referenceSystemIdentifier>
        </gmd:MD_ReferenceSystem>
    </gmd:referenceSystemInfo>
    <gmd:referenceSystemInfo>
        <gmd:MD_ReferenceSystem>
            <gmd:referenceSystemIdentifier>
                <gmd:RS_Identifier>
                    <gmd:code>
                        <gco:CharacterString>VERT_CS["Alicante height",
    VERT_DATUM["Alicante",2000]]</gco:CharacterString>
                    </gmd:code>
                    <gmd:codeSpace>
                        <gco:CharacterString>WKT</gco:CharacterString>
                    </gmd:codeSpace>
                </gmd:RS_Identifier>
            </gmd:referenceSystemIdentifier>
        </gmd:MD_ReferenceSystem>
    </gmd:referenceSystemInfo>
    <gmd:identificationInfo>
        <bag:BAG_DataIdentification>
            <gmd:citation>
                <gmd:CI_Citation>
                    <gmd:title>
                        <gco:CharacterString>Name of dataset input</gco:CharacterString>
                    </gmd:title>
                    <gmd:date>
                        <gmd:CI_Date>
                            <gmd:date>
                                <gco:Date>2008-10-21</gco:Date>
                            </gmd:date>
                            <gmd:dateType>
                                <gmd:CI_DateTypeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#CI_DateTypeCode" codeListValue="creation">creation</gmd:CI_DateTypeCode>
                            </gmd:dateType>
                        </gmd:CI_Date>
                    </gmd:date>
                    <gmd:citedResponsibleParty>
                        <gmd:CI_ResponsibleParty>
                            <gmd:individualName>
                                <gco:CharacterString>Person responsible for input data</gco:CharacterString>
                            </gmd:individualName>
                            <gmd:role>
                                <gmd:CI_RoleCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#CI_RoleCode" codeListValue="originator">originator</gmd:CI_RoleCode>
                            </gmd:role>
                        </gmd:CI_ResponsibleParty>
                    </gmd:citedResponsibleParty>
                </gmd:CI_Citation>
            </gmd:citation>
            <gmd:abstract>
                <gco:CharacterString>Sample Metadata</gco:CharacterString>
            </gmd:abstract>
            <gmd:status>
                <gmd:MD_ProgressCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_ProgressCode" codeListValue="completed">completed</gmd:MD_ProgressCode>
            </gmd:status>
            <gmd:spatialRepresentationType>
                <gmd:MD_SpatialRepresentationTypeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_SpatialRepresentationTypeCode" codeListValue="grid">grid</gmd:MD_SpatialRepresentationTypeCode>
            </gmd:spatialRepresentationType>
            <gmd:language>
                <gmd:LanguageCode codeList="http://www.loc.gov/standards/iso639-2/" codeListValue="eng">eng</gmd:LanguageCode>
            </gmd:language>
            <gmd:characterSet>
                <gmd:MD_CharacterSetCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_CharacterSetCode" codeListValue="utf8">utf8</gmd:MD_CharacterSetCode>
            </gmd:characterSet>
            <gmd:topicCategory>
                <gmd:MD_TopicCategoryCode>elevation</gmd:MD_TopicCategoryCode>
            </gmd:topicCategory>
            <gmd:extent>
                <gmd:EX_Extent>
                    <gmd:geographicElement>
                        <gmd:EX_GeographicBoundingBox>
                            <gmd:westBoundLongitude>
                                <gco:Decimal>-66.371629</gco:Decimal>
                            </gmd:westBoundLongitude>
                            <gmd:eastBoundLongitude>
                                <gco:Decimal>-66.316454</gco:Decimal>
                            </gmd:eastBoundLongitude>
                            <gmd:southBoundLatitude>
                                <gco:Decimal>50.114053</gco:Decimal>
                            </gmd:southBoundLatitude>
                            <gmd:northBoundLatitude>
                                <gco:Decimal>50.180077</gco:Decimal>
                            </gmd:northBoundLatitude>
                        </gmd:EX_GeographicBoundingBox>
                    </gmd:geographicElement>
                </gmd:EX_Extent>
            </gmd:extent>
            <bag:verticalUncertaintyType>
                <bag:BAG_VertUncertCode codeList="http://www.opennavsurf.org/schema/bag/bagCodelists.xml#BAG_VertUncertCode" codeListValue="rawStdDev">rawStdDev</bag:BAG_VertUncertCode>
            </bag:verticalUncertaintyType>
            <bag:depthCorrectionType>
                <bag:BAG_DepthCorrectCode codeList="http://www.opennavsurf.org/schema/bag/bagCodelists.xml#BAG_DepthCorrectCode" codeListValue="trueDepth">trueDepth</bag:BAG_DepthCorrectCode>
            </bag:depthCorrectionType>
            <bag:elevationSolutionGroupType>
                <bag:BAG_OptGroupCode codeList="http://www.opennavsurf.org/schema/bag/bagCodelists.xml#BAG_OptGroupCode" codeListValue="cube">cube</bag:BAG_OptGroupCode>
            </bag:elevationSolutionGroupType>
            <bag:nodeGroupType>
                <bag:BAG_OptGroupCode codeList="http://www.opennavsurf.org/schema/bag/bagCodelists.xml#BAG_OptGroupCode" codeListValue="product">product</bag:BAG_OptGroupCode>
            </bag:nodeGroupType>
        </bag:BAG_DataIdentification>
    </gmd:identificationInfo>
    <gmd:dataQualityInfo>
        <gmd:DQ_DataQuality>
            <gmd:scope>
                <gmd:DQ_Scope>
                    <gmd:level>
                        <gmd:MD_ScopeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_ScopeCode" codeListValue="dataset">dataset</gmd:MD_ScopeCode>
                    </gmd:level>
                </gmd:DQ_Scope>
            </gmd:scope>
            <gmd:lineage>
                <gmd:LI_Lineage>
                    <gmd:processStep>
                        <bag:BAG_ProcessStep>
                            <gmd:description>
                                <gco:CharacterString>List to be determined by WG. I.e. Product Creation</gco:CharacterString>
                            </gmd:description>
                            <gmd:dateTime>
                                <gco:DateTime>2008-10-21T12:21:53</gco:DateTime>
                            </gmd:dateTime>
                            <gmd:processor>
                                <gmd:CI_ResponsibleParty>
                                    <gmd:individualName>
                                        <gco:CharacterString>Name of the processor</gco:CharacterString>
                                    </gmd:individualName>
                                    <gmd:role>
                                        <gmd:CI_RoleCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#CI_RoleCode" codeListValue="processor">processor</gmd:CI_RoleCode>
                                    </gmd:role>
                                </gmd:CI_ResponsibleParty>
                            </gmd:processor>
                            <gmd:source>
                                <gmd:LI_Source>
                                    <gmd:description>
                                        <gco:CharacterString>Source</gco:CharacterString>
                                    </gmd:description>
                                    <gmd:sourceCitation>
                                        <gmd:CI_Citation>
                                            <gmd:title>
                                                <gco:CharacterString>Name of dataset input</gco:CharacterString>
                                            </gmd:title>
                                            <gmd:date>
                                                <gmd:CI_Date>
                                                    <gmd:date gco:nilReason="unknown"/>
                                                    <gmd:dateType>
                                                        <gmd:CI_DateTypeCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#CI_DateTypeCode" codeListValue="creation">creation</gmd:CI_DateTypeCode>
                                                    </gmd:dateType>
                                                </gmd:CI_Date>
                                            </gmd:date>
                                        </gmd:CI_Citation>
                                    </gmd:sourceCitation>
                                </gmd:LI_Source>
                            </gmd:source>
                            <bag:trackingId>
                                <gco:CharacterString>1</gco:CharacterString>
                            </bag:trackingId>
                        </bag:BAG_ProcessStep>
                    </gmd:processStep>
                </gmd:LI_Lineage>
            </gmd:lineage>
        </gmd:DQ_DataQuality>
    </gmd:dataQualityInfo>
    <gmd:metadataConstraints>
        <gmd:MD_LegalConstraints>
            <gmd:useConstraints>
                <gmd:MD_RestrictionCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_RestrictionCode" codeListValue="otherRestrictions">otherRestrictions</gmd:MD_RestrictionCode>
            </gmd:useConstraints>
            <gmd:otherConstraints>
                <gco:CharacterString>some other constraints</gco:CharacterString>
            </gmd:otherConstraints>
        </gmd:MD_LegalConstraints>
    </gmd:metadataConstraints>
    <gmd:metadataConstraints>
        <gmd:MD_SecurityConstraints>
            <gmd:classification>
                <gmd:MD_ClassificationCode codeList="http://www.isotc211.org/2005/resources/Codelist/gmxCodelists.xml#MD_ClassificationCode" codeListValue="unclassified">unclassified</gmd:MD_ClassificationCode>
            </gmd:classification>
            <gmd:userNote>
                <gco:CharacterString>some user node</gco:CharacterString>
            </gmd:userNote>
        </gmd:MD_SecurityConstraints>
    </gmd:metadataConstraints>
</gmi:MI_Metadata>
)"};

}  // namespace BAG

bool operator==(
    const VRTrackingList::value_type& lhs,
    const VRTrackingList::value_type& rhs)
{
    return lhs.row == rhs.row && lhs.col == rhs.col &&
        lhs.sub_row == rhs.sub_row && lhs.sub_col == rhs.sub_col &&
        lhs.depth == rhs.depth && lhs.uncertainty == rhs.uncertainty &&
        lhs.track_code == rhs.track_code && lhs.list_series == rhs.list_series;
}

//  VRTrackingList(const Dataset& dataset, int compressionLevel);
//  explicit VRTrackingList(Dataset& dataset);
TEST_CASE("test VR tracking list construction (create, open)", "[vrtrackinglist][constructor][destructor]")
{
    const TestUtils::RandomFileGuard tmpFileName;

    const VRTrackingList::value_type kExpectedItem0{1, 2, 11, 22, 3.4f, 5.6f, 7, 8};
    const VRTrackingList::value_type kExpectedItem1{9, 8, 101, 202, 7.6f, 5.4f, 3, 2};

    {
        BAG::Metadata metadata;
        metadata.loadFromBuffer(kMetadataXML);

        constexpr uint64_t kChunkSize = 100;
        constexpr int kCompressionLevel = 6;
        const auto pDataset = Dataset::create(tmpFileName, std::move(metadata),
            kChunkSize, kCompressionLevel);

        UNSCOPED_INFO("Check dataset was created successfully.");
        REQUIRE(pDataset);

        {
            auto trackingList = pDataset->getVRTrackingList();
            UNSCOPED_INFO("Check no VR tracking list exists.");
            REQUIRE(trackingList == nullptr);

            pDataset->createVR(kChunkSize, kCompressionLevel, false);
            trackingList = pDataset->getVRTrackingList();
            UNSCOPED_INFO("Check VR tracking list was created successfully.");
            REQUIRE(trackingList);

            UNSCOPED_INFO("Check no items are in the tracking list after creation.");
            CHECK(trackingList->empty());

            // Add 1 record.
            trackingList->emplace_back(kExpectedItem0);

            UNSCOPED_INFO("Check one item was stored.");
            CHECK(trackingList->size() == 1);

            UNSCOPED_INFO("Check writing to the HDF5 does not throw.");
            REQUIRE_NOTHROW(trackingList->write());
        }

        {
            // Read the expected record.
            const auto trackingList = pDataset->getVRTrackingList();
            REQUIRE(trackingList);

            UNSCOPED_INFO("Check one item was read.");
            CHECK(trackingList->size() == 1);

            UNSCOPED_INFO("Check the expected values are in the item.");
            CHECK(kExpectedItem0 == trackingList->front());
        }

        {
            // Add a second record to an existing TrackingList.
            auto trackingList = pDataset->getVRTrackingList();
            REQUIRE(trackingList);

            UNSCOPED_INFO("Check one item was read.");
            CHECK(trackingList->size() == 1);

            trackingList->emplace_back(kExpectedItem1);
            CHECK(trackingList->size() == 2);

            UNSCOPED_INFO("Check writing to the HDF5 does not throw.");
            REQUIRE_NOTHROW(trackingList->write());
        }

        const auto trackingList = pDataset->getVRTrackingList();

        UNSCOPED_INFO("Check the expected values are in the item.");
        CHECK(trackingList->size() == 2);

        UNSCOPED_INFO("Check the first expected values are exactly the same.");
        CHECK(kExpectedItem0 == (*trackingList)[0]);

        UNSCOPED_INFO("Check the second expected values are exactly the same.");
        CHECK(kExpectedItem1 == (*trackingList)[1]);
    }

    const auto pDataset = Dataset::open(tmpFileName, BAG_OPEN_READONLY);

    UNSCOPED_INFO("Check dataset was read successfully.");
    REQUIRE(pDataset);

    const auto trackingList = pDataset->getVRTrackingList();

    UNSCOPED_INFO("Check the expected number of items in the tracking list are there.");
    constexpr size_t kExpectedNumItems = 2;
    REQUIRE(trackingList->size() == kExpectedNumItems);

    UNSCOPED_INFO("Check the first expected values are exactly the same.");
    CHECK(kExpectedItem0 == (*trackingList)[0]);

    UNSCOPED_INFO("Check the second expected values are exactly the same.");
    CHECK(kExpectedItem1 == (*trackingList)[1]);
}

