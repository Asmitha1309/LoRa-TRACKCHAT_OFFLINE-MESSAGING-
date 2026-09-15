#ifndef LORA_NODE_HEALTH_MODEL_H
#define LORA_NODE_HEALTH_MODEL_H

// ============================================================
// LoRa Node Health TinyML Model
// Algorithm : Decision Tree
// Max Depth : 5
// Features  : 6
// Validation Accuracy : 94.67%
// ============================================================
//
// Feature order:
// 0 = RSSI
// 1 = SNR
// 2 = Distance
// 3 = Battery_Level
// 4 = Packet_Loss
// 5 = Latency
//
// Output:
// 0 = ABNORMAL
// 1 = NORMAL
// ============================================================

static inline int predictNodeHealth(const float features[6])
{
    if (features[1] <= -0.096080f) {
        if (features[4] <= 8.061706f) {
            if (features[0] <= -94.462734f) {
                if (features[5] <= 298.820694f) {
                    if (features[2] <= 2.011460f) {
                        return 1;
                    } else {
                        return 0;
                    }
                } else {
                    return 0;
                }
            } else {
                if (features[5] <= 298.307785f) {
                    if (features[3] <= 28.429331f) {
                        return 1;
                    } else {
                        return 1;
                    }
                } else {
                    if (features[2] <= 1.996754f) {
                        return 1;
                    } else {
                        return 0;
                    }
                }
            }
        } else {
            if (features[5] <= 287.810349f) {
                if (features[2] <= 1.963139f) {
                    if (features[0] <= -95.581215f) {
                        return 0;
                    } else {
                        return 1;
                    }
                } else {
                    return 0;
                }
            } else {
                if (features[0] <= -41.903578f) {
                    return 0;
                } else {
                    if (features[0] <= -41.632841f) {
                        return 1;
                    } else {
                        return 0;
                    }
                }
            }
        }
    } else {
        if (features[2] <= 1.999970f) {
            if (features[0] <= -94.671329f) {
                if (features[5] <= 300.300308f) {
                    if (features[3] <= 28.986435f) {
                        return 0;
                    } else {
                        return 1;
                    }
                } else {
                    if (features[4] <= 8.017349f) {
                        return 1;
                    } else {
                        return 0;
                    }
                }
            } else {
                if (features[3] <= 29.665026f) {
                    if (features[5] <= 306.959457f) {
                        return 1;
                    } else {
                        return 1;
                    }
                } else {
                    if (features[1] <= -0.005689f) {
                        return 1;
                    } else {
                        return 1;
                    }
                }
            }
        } else {
            if (features[5] <= 299.522583f) {
                if (features[0] <= -94.763290f) {
                    if (features[4] <= 8.093522f) {
                        return 1;
                    } else {
                        return 0;
                    }
                } else {
                    if (features[3] <= 29.864024f) {
                        return 0;
                    } else {
                        return 1;
                    }
                }
            } else {
                if (features[4] <= 7.872298f) {
                    if (features[0] <= -94.268520f) {
                        return 0;
                    } else {
                        return 1;
                    }
                } else {
                    return 0;
                }
            }
        }
    }
}

#endif