#include "cbicaCmdParser.h"
#include "cbicaLogging.h"
#include "cbicaITKSafeImageIO.h"
#include "cbicaUtilities.h"
#include "cbicaITKUtilities.h"
#include "DicomMetadataReader.h"

#include "itkBoundingBox.h"
#include "itkPointSet.h"
#include "itkBinaryThresholdImageFilter.h"

//! Detail the available algorithms to make it easier to initialize
enum AvailableAlgorithms
{
  None,
  Resize,
  Resample,
  SanityCheck,
  Information,
  Casting,
  UniqueValues,
  TestComparison,
  BoundingBox,
  CreateMask,
  ChangeValue,
  DicomLoadTesting,
  Dicom2Nifti
};

int requestedAlgorithm = 0;

std::string inputImageFile, inputMaskFile, outputImageFile, targetImageFile, resamplingInterpolator;
std::string dicomFolderPath;
size_t resize = 100;
int testRadius = 0, testNumber = 0;
float testThresh = 0.0, testAvgDiff = 0.0, lowerThreshold = 1, upperThreshold = std::numeric_limits<double>::max();
std::string changeOldValues, changeNewValues;
float resamplingResolution = 1.0;

bool uniqueValsSort = true, boundingBoxIsotropic = true;

template< class TImageType >
int algorithmsRunner()
{
  if (requestedAlgorithm == Resize)
  {
    auto outputImage = cbica::ResizeImage< TImageType >(cbica::ReadImage< TImageType >(inputImageFile), resize, resamplingInterpolator);
    cbica::WriteImage< TImageType >(outputImage, outputImageFile);

    std::cout << "Resizing by a factor of " << resize << "% completed.\n";
    return EXIT_SUCCESS;
  }

  if (requestedAlgorithm == Resample)
  {
    auto outputImage = cbica::ResampleImage< TImageType >(cbica::ReadImage< TImageType >(inputImageFile), resamplingResolution, resamplingInterpolator);
    cbica::WriteImage< TImageType >(outputImage, outputImageFile);

    std::cout << "Resampled image to isotropic resolution of '" << resamplingResolution << "' using interpolator '" << resamplingInterpolator << "'.\n";
    return EXIT_SUCCESS;
  }

  if (requestedAlgorithm == UniqueValues)
  {
    bool sort = true;
    if (uniqueValsSort == 0)
    {
      sort = false;
    }
    auto uniqueValues = cbica::GetUniqueValuesInImage< TImageType >(cbica::ReadImage < TImageType >(inputImageFile), sort);

    if (!uniqueValues.empty())
    {
      std::cout << "Unique values:\n";
      for (size_t i = 0; i < uniqueValues.size(); i++)
      {
        std::cout << cbica::to_string_precision(uniqueValues[i]) << "\n";
      }
    }
    return EXIT_SUCCESS;
  }

  if (requestedAlgorithm == CreateMask)
  {
    auto thresholder = itk::BinaryThresholdImageFilter< TImageType, TImageType >::New();
    thresholder->SetInput(cbica::ReadImage< TImageType >(inputImageFile));
    thresholder->SetLowerThreshold(lowerThreshold);
    thresholder->SetUpperThreshold(upperThreshold);
    thresholder->SetOutsideValue(0);
    thresholder->SetInsideValue(1);
    thresholder->Update();

    cbica::WriteImage< TImageType >(thresholder->GetOutput(), outputImageFile);
    std::cout << "Create Mask completed.\n";
    return EXIT_SUCCESS;
  }

  if (requestedAlgorithm == DicomLoadTesting)
  {
    auto readDicomImage = cbica::ReadImage< TImageType >(dicomFolderPath);
    if (!readDicomImage)
    {
      std::cout << "Dicom Load Failed" << std::endl;
      return EXIT_FAILURE;
    }
    auto inputNiftiImage = cbica::ReadImage< TImageType >(inputImageFile);

    //cbica::WriteImage< TImageType>(inputNiftiImage, "readNifti.nii.gz");

    bool differenceFailed = false;

    auto diffFilter = itk::Testing::ComparisonImageFilter< TImageType, TImageType >::New();
    diffFilter->SetValidInput(inputNiftiImage);
    diffFilter->SetTestInput(readDicomImage);
    auto size = inputNiftiImage->GetBufferedRegion().GetSize();
    diffFilter->VerifyInputInformationOn();
    diffFilter->SetDifferenceThreshold(testThresh);
    diffFilter->SetToleranceRadius(testRadius);
    diffFilter->UpdateLargestPossibleRegion();

    size_t totalSize = 1;
    for (size_t d = 0; d < TImageType::ImageDimension; d++)
    {
      totalSize *= size[d];
    }

    const double averageIntensityDifference = diffFilter->GetTotalDifference();
    const unsigned long numberOfPixelsWithDifferences = diffFilter->GetNumberOfPixelsWithDifferences();

    std::cout << "Total Voxels in Image       : " << totalSize << "\n";
    std::cout << "Number of Difference Voxels : " << diffFilter->GetNumberOfPixelsWithDifferences() << "\n";
    std::cout << "Percentage of Diff Voxels   : " << diffFilter->GetNumberOfPixelsWithDifferences() * 100 / totalSize << "\n";
    std::cout << "Minimum Intensity Difference: " << diffFilter->GetMinimumDifference() << "\n";
    std::cout << "Maximum Intensity Difference: " << diffFilter->GetMaximumDifference() << "\n";
    std::cout << "Average Intensity Difference: " << diffFilter->GetMeanDifference() << "\n";
    std::cout << "Overall Intensity Difference: " << diffFilter->GetTotalDifference() << "\n";

    int numberOfPixelsTolerance = 70;
    if (averageIntensityDifference > 0.0)
    {
      if (static_cast<int>(numberOfPixelsWithDifferences) >
        numberOfPixelsTolerance)
      {
        differenceFailed = true;
      }
      else
      {
        differenceFailed = false;
      }
    }
    else
    {
      differenceFailed = false;
    }
    return differenceFailed;
    //return EXIT_FAILURE;
  }

  if (requestedAlgorithm == Dicom2Nifti)
  {
    auto readDicomImage = cbica::ReadImage< TImageType >(inputImageFile);
    if (!readDicomImage)
    {
      std::cout << "Dicom Load Failed" << std::endl;
      return EXIT_FAILURE;
    }

    cbica::WriteImage< TImageType>(readDicomImage, outputImageFile);

    if (!targetImageFile.empty())
    {
      if (!cbica::ImageSanityCheck< TImageType >(readDicomImage, cbica::ReadImage< TImageType >(targetImageFile)))
      {
        std::cerr << "Input image and target image physical space mismatch.\n";
        return EXIT_FAILURE;
      }
      else
      {
        auto diffFilter = itk::Testing::ComparisonImageFilter< TImageType, TImageType >::New();
        auto inputImage = cbica::ReadImage< TImageType >(inputImageFile);
        auto size = inputImage->GetBufferedRegion().GetSize();
        diffFilter->SetValidInput(cbica::ReadImage< TImageType >(targetImageFile));
        diffFilter->SetTestInput(inputImage);
        diffFilter->VerifyInputInformationOn();
        diffFilter->SetDifferenceThreshold(testThresh);
        diffFilter->SetToleranceRadius(testRadius);
        diffFilter->UpdateLargestPossibleRegion();

        size_t totalSize = 1;
        for (size_t d = 0; d < TImageType::ImageDimension; d++)
        {
          totalSize *= size[d];
        }

        std::cout << "Total Voxels/Pixels in Image: " << totalSize << "\n";
        std::cout << "Number of Difference Voxels : " << diffFilter->GetNumberOfPixelsWithDifferences() << "\n";
        std::cout << "Percentage of Diff Voxels   : " << diffFilter->GetNumberOfPixelsWithDifferences() * 100 / totalSize << "\n";
        std::cout << "Minimum Intensity Difference: " << diffFilter->GetMinimumDifference() << "\n";
        std::cout << "Maximum Intensity Difference: " << diffFilter->GetMaximumDifference() << "\n";
        std::cout << "Average Intensity Difference: " << diffFilter->GetMeanDifference() << "\n";
        std::cout << "Overall Intensity Difference: " << diffFilter->GetTotalDifference() << "\n";
      }

      return EXIT_SUCCESS;
    }
  }

  if (requestedAlgorithm == ChangeValue)
  {
    auto outputImage = cbica::ChangeImageValues< TImageType >(cbica::ReadImage< TImageType >(inputImageFile), changeOldValues, changeNewValues);

    if (!outputImage.IsNull())
    {
      cbica::WriteImage< TImageType >(outputImage, outputImageFile);
      std::cout << "Create Mask completed.\n";
      return EXIT_SUCCESS;
    }
    else
    {
      return EXIT_FAILURE;
    }
  }

  if (requestedAlgorithm == Casting)
  {
    if (targetImageFile == "uchar")
    {
      using DefaultPixelType = unsigned char;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else if (targetImageFile == "char")
    {
      using DefaultPixelType = char;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else if (targetImageFile == "uint")
    {
      using DefaultPixelType = unsigned int;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else if (targetImageFile == "int")
    {
      using DefaultPixelType = int;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else if (targetImageFile == "ulong")
    {
      using DefaultPixelType = unsigned long;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else if (targetImageFile == "long")
    {
      using DefaultPixelType = long;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else if (targetImageFile == "ulonglong")
    {
      using DefaultPixelType = unsigned long long;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else if (targetImageFile == "longlong")
    {
      using DefaultPixelType = long long;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else if (targetImageFile == "float")
    {
      using DefaultPixelType = float;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else if (targetImageFile == "double")
    {
      using DefaultPixelType = double;
      using CurrentImageType = itk::Image< DefaultPixelType, TImageType::ImageDimension >;
      cbica::WriteImage< CurrentImageType >(cbica::ReadImage< CurrentImageType >(inputImageFile), outputImageFile);
    }
    else
    {
      std::cerr << "Undefined pixel type cast requested, cannot process.\n";
      return EXIT_FAILURE;
    }

    std::cout << "Casting completed.\n";
    return EXIT_SUCCESS;
  }

  if (requestedAlgorithm == TestComparison)
  {
    auto diffFilter = itk::Testing::ComparisonImageFilter< TImageType, TImageType >::New();
    auto inputImage = cbica::ReadImage< TImageType >(inputImageFile);
    auto size = inputImage->GetBufferedRegion().GetSize();
    diffFilter->SetValidInput(cbica::ReadImage< TImageType >(targetImageFile));
    diffFilter->SetTestInput(inputImage);
    if (cbica::ImageSanityCheck(inputImageFile, targetImageFile))
    {
      diffFilter->VerifyInputInformationOn();
      diffFilter->SetDifferenceThreshold(testThresh);
      diffFilter->SetToleranceRadius(testRadius);
      diffFilter->UpdateLargestPossibleRegion();

      size_t totalSize = 1;
      for (size_t d = 0; d < TImageType::ImageDimension; d++)
      {
        totalSize *= size[d];
      }

      std::cout << "Total Voxels/Pixels in Image: " << totalSize << "\n";
      std::cout << "Number of Difference Voxels : " << diffFilter->GetNumberOfPixelsWithDifferences() << "\n";
      std::cout << "Percentage of Diff Voxels   : " << diffFilter->GetNumberOfPixelsWithDifferences() * 100 / totalSize << "\n";
      std::cout << "Minimum Intensity Difference: " << diffFilter->GetMinimumDifference() << "\n";
      std::cout << "Maximum Intensity Difference: " << diffFilter->GetMaximumDifference() << "\n";
      std::cout << "Average Intensity Difference: " << diffFilter->GetMeanDifference() << "\n";
      std::cout << "Overall Intensity Difference: " << diffFilter->GetTotalDifference() << "\n";
    }
    else
    {
      std::cerr << "Images are in different spaces (size/origin/spacing mismatch).\n";
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  if (requestedAlgorithm == BoundingBox)
  {
    if (!cbica::ImageSanityCheck(inputImageFile, targetImageFile))
    {
      std::cerr << "Input image and mask are in different spaces, cannot compute bounding box.\n";
      return EXIT_FAILURE;
    }

    auto inputImage = cbica::ReadImage< TImageType >(inputImageFile);
    auto maskImage = cbica::ReadImage< TImageType >(targetImageFile);
    auto outputImage = cbica::CreateImage< TImageType >(maskImage);
    auto size = maskImage->GetBufferedRegion().GetSize();

    auto nonZeroIndeces = cbica::GetNonZeroIndeces< TImageType >(maskImage);

    using PointSetType = itk::PointSet< typename TImageType::PixelType, TImageType::ImageDimension >;
    using PointIdentifier = typename PointSetType::PointIdentifier;
    using PointType = typename PointSetType::PointType;

    auto pointSet = PointSetType::New();
    auto points = pointSet->GetPoints();

    for (size_t i = 0; i < nonZeroIndeces.size(); i++)
    {
      PointType currentPoint;
      for (size_t d = 0; d < TImageType::ImageDimension; d++)
      {
        currentPoint[d] = nonZeroIndeces[i][d];
      }
      points->InsertElement(i, currentPoint);
    }

    auto boundingBoxCalculator = itk::BoundingBox< PointIdentifier, TImageType::ImageDimension, typename TImageType::PixelType >::New();
    boundingBoxCalculator->SetPoints(points);
    boundingBoxCalculator->ComputeBoundingBox();

    auto max = boundingBoxCalculator->GetMaximum();
    auto min = boundingBoxCalculator->GetMinimum();
    auto center = boundingBoxCalculator->GetCenter();

    std::vector< float > distances;

    for (size_t d = 0; d < TImageType::ImageDimension; d++)
    {
      distances.push_back(std::abs(max[d] - min[d]));
    }

    size_t maxDist = 0;
    for (size_t i = 1; i < TImageType::ImageDimension; i++)
    {
      if (distances[maxDist] < distances[i])
      {
        maxDist = i;
      }
    }

    auto minFromCenter = center, maxFromCenter = center;
    for (size_t d = 0; d < TImageType::ImageDimension; d++)
    {
      size_t isoTropicCheck = maxDist;
      if (!boundingBoxIsotropic)
      {
        isoTropicCheck = d;
      }
      minFromCenter[d] = std::round(minFromCenter[d] - distances[isoTropicCheck] / 2);
      if (minFromCenter[d] < 0)
      {
        minFromCenter[d] = 0;
      }

      maxFromCenter[d] = std::round(maxFromCenter[d] + distances[isoTropicCheck] / 2);
      if (maxFromCenter[d] > size[d])
      {
        maxFromCenter[d] = size[d];
      }
    }

    itk::ImageRegionConstIterator< TImageType > iterator(inputImage, inputImage->GetBufferedRegion());
    itk::ImageRegionIterator< TImageType > outputIterator(outputImage, outputImage->GetBufferedRegion());

    switch (TImageType::ImageDimension)
    {
    case 2:
    {
      for (size_t x = minFromCenter[0]; x < maxFromCenter[0]; x++)
      {
        for (size_t y = minFromCenter[1]; x < maxFromCenter[1]; x++)
        {
          typename TImageType::IndexType currentIndex;
          currentIndex[0] = x;
          currentIndex[1] = y;

          iterator.SetIndex(currentIndex);
          outputIterator.SetIndex(currentIndex);
          outputIterator.Set(iterator.Get());
        }
      }
      break;
    }
    case 3:
    {
      for (size_t x = minFromCenter[0]; x < maxFromCenter[0]; x++)
      {
        for (size_t y = minFromCenter[1]; y < maxFromCenter[1]; y++)
        {
          for (size_t z = minFromCenter[2]; z < maxFromCenter[2]; z++)
          {
            typename TImageType::IndexType currentIndex;
            currentIndex[0] = x;
            currentIndex[1] = y;
            currentIndex[2] = z;

            iterator.SetIndex(currentIndex);
            outputIterator.SetIndex(currentIndex);
            outputIterator.Set(iterator.Get());
          }
        }
      }
      break;
    }
    default:
      std::cerr << "Images other than 2D or 3D are not supported, right now.\n";
      return EXIT_FAILURE;
    }

    cbica::WriteImage< TImageType >(outputImage, outputImageFile);
  }
  
  return EXIT_SUCCESS;
}


int main(int argc, char** argv)
{
  cbica::CmdParser parser(argc, argv, "Utilities");

  parser.addOptionalParameter("i", "inputImage", cbica::Parameter::STRING, "File or Dir", "Input Image (all CaPTk supported images) for processing", "Directory to a single series DICOM only");
  parser.addOptionalParameter("m", "maskImage", cbica::Parameter::STRING, "File or Dir", "Input Mask (all CaPTk supported images) for processing", "Directory to a single series DICOM only");
  parser.addOptionalParameter("o", "outputImage", cbica::Parameter::FILE, "NIfTI", "Output Image for processing");
  parser.addOptionalParameter("df", "dicomDirectory", cbica::Parameter::DIRECTORY, "none", "Absolute path of directory containing single dicom series");
  parser.addOptionalParameter("r", "resize", cbica::Parameter::INTEGER, "10-500", "Resize an image based on the resizing factor given", "Example: -r 150 resizes inputImage by 150%", "Defaults to 100, i.e., no resizing", "Resampling can be done on image with 100");
  parser.addOptionalParameter("rr", "resizeResolution", cbica::Parameter::FLOAT, "0-10", "[Resample] Isotropic resolution of the voxels/pixels to change to", "Resize value needs to be 100", "Defaults to 1.0");
  parser.addOptionalParameter("ri", "resizeInterp", cbica::Parameter::STRING, "NEAREST:LINEAR:BSPLINE:BICUBIC", "The interpolation type to use for resampling or resizing", "Defaults to LINEAR");
  parser.addOptionalParameter("s", "sanityCheck", cbica::Parameter::FILE, "NIfTI Reference", "Do sanity check of inputImage with the file provided in with this parameter", "Performs checks on size, origin & spacing", "Pass the target image after '-s'");
  parser.addOptionalParameter("inf", "information", cbica::Parameter::BOOLEAN, "true or false", "Output the information in inputImage", "If DICOM file is detected, the tags are written out");
  parser.addOptionalParameter("c", "cast", cbica::Parameter::STRING, "(u)char, (u)int, (u)long, (u)longlong, float, double", "Change the input image type", "Examples: '-c uchar', '-c float', '-c longlong'");
  parser.addOptionalParameter("un", "uniqueVals", cbica::Parameter::BOOLEAN, "true or false", "Output the unique values in the inputImage", "Pass value '1' for ascending sort or '0' for no sort", "Defaults to '1'");
  parser.addOptionalParameter("b", "boundingBox", cbica::Parameter::FILE, "NIfTI Mask", "Extracts the smallest bounding box around the mask file", "With respect to inputImage", "Writes to outputImage");
  parser.addOptionalParameter("bi", "boundingIso", cbica::Parameter::BOOLEAN, "Isotropic Box or not", "Whether the bounding box is Isotropic or not", "Defaults to true");
  parser.addOptionalParameter("tb", "testBase", cbica::Parameter::FILE, "NIfTI Reference", "Baseline image to compare inputImage with");
  parser.addOptionalParameter("tr", "testRadius", cbica::Parameter::INTEGER, "0-10", "Maximum distance away to look for a matching pixel", "Defaults to 0");
  parser.addOptionalParameter("tt", "testThresh", cbica::Parameter::FLOAT, "0-5", "Minimum threshold for pixels to be different", "Defaults to 0.0");
  parser.addOptionalParameter("cm", "createMask", cbica::Parameter::STRING, "N.A.", "Create a binary mask out of a provided (float) thresholds","Format: -cm lower,upper", "Output is 1 if value >= lower or <= upper", "Defaults to 1,Max");
  parser.addOptionalParameter("cv", "changeValue", cbica::Parameter::STRING, "N.A.", "Change the specified pixel/voxel value", "Format: -cv oldValue1xoldValue2,newValue1xnewValue2", "Can be used for multiple number of value changes", "Defaults to 3,4");
  parser.addOptionalParameter("d2n", "dicom2Nifti", cbica::Parameter::FILE, "NIfTI Reference", "If path to reference is present, then image comparison is done", "Use '-i' to pass input DICOM image", "Use '-o' to pass output image file");

  parser.addExampleUsage("-i C:/test.nii.gz -o C:/test_int.nii.gz -c int", "Cast an image pixel-by-pixel to a signed integer");
  parser.addExampleUsage("-i C:/test.nii.gz -o C:/test_75.nii.gz -r 75 -ri linear", "Resize an image by 75% using linear interpolation");
  parser.addExampleUsage("-i C:/test.nii.gz -inf", "Prints out image information to console (for DICOMs, this does a full dump of the tags)");
  parser.addExampleUsage("-i C:/test/1.dcm -o C:/test.nii.gz -d2n C:/test_reference.nii.gz", "DICOM to NIfTI conversion and do sanity check of the converted image with the reference image");

  parser.addApplicationDescription("This application has various utilities that can be used for constructing pipelines around CaPTk's functionalities. Please add feature requests on the CaPTk GitHub page at https://github.com/CBICA/CaPTk.");

  if (parser.isPresent("i"))
  {
    parser.getParameterValue("i", inputImageFile);
  }
  if (parser.isPresent("m"))
  {
    parser.getParameterValue("m", inputMaskFile);
  }
  if (parser.isPresent("o"))
  {
    parser.getParameterValue("o", outputImageFile);
  }
  if(parser.isPresent("df"))
  {
    requestedAlgorithm = DicomLoadTesting;
    parser.getParameterValue("df", dicomFolderPath);
    parser.getParameterValue("i", inputImageFile);
    parser.getParameterValue("tt", testThresh);
  }
  if (parser.isPresent("d2n"))
  {
    requestedAlgorithm = Dicom2Nifti;
    parser.getParameterValue("d2n", targetImageFile);
    parser.getParameterValue("o", outputImageFile);
  }
  if (parser.isPresent("r"))
  {
    parser.getParameterValue("r", resize);
    if (resize != 100)
    {
      requestedAlgorithm = Resize;
    }
    else
    {
      requestedAlgorithm = Resample;
      if (parser.isPresent("rr"))
      {
        parser.getParameterValue("rr", resamplingResolution);
      }
    }
    if (parser.isPresent("ri"))
    {
      parser.getParameterValue("ri", resamplingInterpolator);
    }
  }
  if (parser.isPresent("s"))
  {
    parser.getParameterValue("s", targetImageFile);
    requestedAlgorithm = SanityCheck;
  }
  if (parser.isPresent("inf"))
  {
    requestedAlgorithm = Information;
  }
  if (parser.isPresent("c"))
  {
    parser.getParameterValue("c", targetImageFile);
    requestedAlgorithm = Casting;
  }
  if (parser.isPresent("un"))
  {
    parser.getParameterValue("un", uniqueValsSort);
    requestedAlgorithm = UniqueValues;
  }
  if (parser.isPresent("tb"))
  {
    parser.getParameterValue("tb", targetImageFile);
    requestedAlgorithm = TestComparison;
    if (parser.isPresent("tr"))
    {
      parser.getParameterValue("tr", testRadius);
    }
    if (parser.isPresent("tt"))
    {
      parser.getParameterValue("tt", testThresh);
    }
  }
  if (parser.isPresent("b"))
  {
    parser.getParameterValue("b", targetImageFile);
    requestedAlgorithm = BoundingBox;
    if (parser.isPresent("bi"))
    {
      parser.getParameterValue("bi", boundingBoxIsotropic);
    }
  }

  if (parser.isPresent("cm"))
  {
    std::string temp;
    parser.getParameterValue("cm", temp);
    if (!temp.empty())
    {
      auto temp2 = cbica::stringSplit(temp, ",");
      lowerThreshold = std::atof(temp2[0].c_str());
      if (temp2.size() == 2)
      {
        upperThreshold = std::atof(temp2[1].c_str());
      }
    }

    requestedAlgorithm = CreateMask;
  }

  if (parser.isPresent("cv"))
  {
    std::string temp;
    parser.getParameterValue("cv", temp);
    if (!temp.empty())
    {
      auto temp2 = cbica::stringSplit(temp, ",");
      if (temp2.size() != 2)
      {
        std::cerr << "Change value needs 2 values in the format '-cv oldValue,newValue' to work.\n";
        return EXIT_FAILURE;
      }
      changeOldValues = temp2[0];
      changeNewValues = temp2[1];
    }

    requestedAlgorithm = ChangeValue;
  }

  // this doesn't need any template initialization
  if (requestedAlgorithm == SanityCheck)
  {
    if (cbica::ImageSanityCheck(inputImageFile, targetImageFile))
    {
      std::cout << "Images are in the same space.\n";
      return EXIT_SUCCESS;
    }
    else
    {
      std::cerr << "Images are in different spaces.\n";
      return EXIT_FAILURE;
    }
  }
  auto inputImageInfo = cbica::ImageInfo(inputImageFile);

  if (requestedAlgorithm == Information)
  {
    if (cbica::IsDicom(inputImageFile)) // if dicom file
    {
      std::cout << "DICOM file detected, will print out all tags.\n";
      DicomMetadataReader reader;
      reader.SetFilePath(inputImageFile);
      bool readStatus = reader.ReadMetaData();
      if (!readStatus)
      {
        std::cout << "Could not read dicom image.\n";
        return EXIT_FAILURE;
      }

      auto readMap = reader.GetMetaDataMap();
      std::pair<std::string, std::string> labelValuePair;
      std::cout << "Tag" << "," << "Description" << "," << "Value" << std::endl;
      for (auto itr = readMap.begin(); itr != readMap.end(); ++itr)
      {
        labelValuePair = itr->second;
        std::cout << itr->first.c_str() << ","
          << labelValuePair.first.c_str() << ","
          << labelValuePair.second.c_str() << "\n";
      }
    }
    else
    {
      std::cout << "Non-DICOM file detected, will print out ITK Image information.\n";
      auto dims = inputImageInfo.GetImageDimensions();
      auto size = inputImageInfo.GetImageSize();
      auto origin = inputImageInfo.GetImageOrigins();
      auto spacing = inputImageInfo.GetImageSpacings();
      auto size_string = std::to_string(size[0]);
      auto origin_string = std::to_string(origin[0]);
      auto spacing_string = std::to_string(spacing[0]);
      size_t totalSize = size[0];
      for (size_t i = 1; i < dims; i++)
      {
        size_string += "x" + std::to_string(size[i]);
        origin_string += "x" + std::to_string(origin[i]);
        spacing_string += "x" + std::to_string(spacing[i]);
        totalSize *= size[i];
      }
      std::cout << "Property,Value\n";
      std::cout << "Dimensions," << dims << "\n";
      std::cout << "Size," << size_string << "\n";
      std::cout << "Total," << totalSize << "\n";
      std::cout << "Origin," << origin_string << "\n";
      std::cout << "Spacing," << spacing_string << "\n";
      std::cout << "Component," << inputImageInfo.GetComponentTypeAsString() << "\n";
      std::cout << "Pixel Type," << inputImageInfo.GetPixelTypeAsString() << "\n"; 
    }
    return EXIT_SUCCESS;
  }

  switch (inputImageInfo.GetImageDimensions())
  {
  case 2:
  {
    using ImageType = itk::Image< float, 2 >;
    return algorithmsRunner< ImageType >();

    break;
  }
  case 3:
  {
    using ImageType = itk::Image< float, 3 >;
    return algorithmsRunner< ImageType >();

    break;
  }
  case 4:
  {
    using ImageType = itk::Image< float, 4 >;
    return algorithmsRunner< ImageType >();

    break;
  }
  default:
    std::cerr << "Supplied image has an unsupported dimension of '" << inputImageInfo.GetImageDimensions() << "'; only 2, 3 and 4 D images are supported.\n";
    return EXIT_FAILURE; // exiting here because no further processing should be done on the image
  }

  return EXIT_SUCCESS;
}