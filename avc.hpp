#pragma once
/*
aligned(8) class AVCDecoderConfigurationRecord {
 unsigned int(8) configurationVersion = 1;
 unsigned int(8) AVCProfileIndication;
 unsigned int(8) profile_compatibility;
 unsigned int(8) AVCLevelIndication;
 bit(6) reserved = ‘111111’b;
 unsigned int(2) lengthSizeMinusOne;
 bit(3) reserved = ‘111’b;
 unsigned int(5) numOfSequenceParameterSets;
 for (i=0; i< numOfSequenceParameterSets; i++) {
 unsigned int(16) sequenceParameterSetLength ;
 bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
 }
 unsigned int(8) numOfPictureParameterSets;
 for (i=0; i< numOfPictureParameterSets; i++) {
 unsigned int(16) pictureParameterSetLength;
 bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
 }
}
*/