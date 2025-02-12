// Copyright (c) 2011 Nordic Semiconductor. All Rights Reserved.
//
// The information contained herein is property of Nordic Semiconductor ASA.
// Terms and conditions of usage are described in detail in // NORDIC SEMICONDUCTOR SOFTWARE DEVELOPMENT KIT LICENSE AGREEMENT.
//
// Licensees are granted free, non-transferable use of the information.
// NO WARRANTY of ANY KIND is provided.
// This heading must NOT be removed from the file.
//
//
//  WFFSSensorVCBase.h
//  NordicSemiDemo
//
//  Created by Michael Moore on 6/11/10.
//

#import <UIKit/UIKit.h>
#import "WFSensorCommonViewController.h"
#import <WFConnector/WFAntFS.h>


@class WFAntFileManager;


@interface WFFSSensorVCBase : WFSensorCommonViewController <WFAntFileManagerDelegate>
{
	WFAntFileManager* afsFileManager;
	UCHAR aucDevicePassword[WF_ANTFS_PASSWORD_MAX_LENGTH];
	UCHAR ucDevicePasswordLength;
	WFAntFSDeviceType_t deviceType;
	BOOL bHistoryLoaded;
	
	UIButton* historyButton;
	UIButton* refreshButton;
	UILabel* refreshdateLabel;
	UILabel* refreshtimeLabel;
}


@property (retain, nonatomic) IBOutlet UIButton* historyButton;
@property (retain, nonatomic) IBOutlet UIButton* refreshButton;
@property (retain, nonatomic) IBOutlet UILabel* refreshdateLabel;
@property (retain, nonatomic) IBOutlet UILabel* refreshtimeLabel;


- (IBAction)historyClicked:(id)sender;
- (IBAction)refreshClicked:(id)sender;

- (void)connectToDevice;
- (void)disconnectDevice;
- (void)displayLastRecord;
- (NSArray*)getFitRecords:(NSString*)filePath;
- (void)loadPasskey;
- (void)resetDisplayFS;
- (void)updateDisplay:(NSArray*)fitRecords;

@end
