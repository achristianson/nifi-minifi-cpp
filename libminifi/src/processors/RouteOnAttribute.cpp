/**
 * @file RouteOnAttribute.cpp
 * RouteOnAttribute class implementation
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "processors/RouteOnAttribute.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sstream>
#include <iostream>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
core::Property RouteOnAttribute::malwareDetected(
    "malwareDetected Attribute", "If the 'malwareDetected' attribute is set to 'true', then will route FlowFile to MalwareDetected relationship.  Value of property should match value of attribute for matching to occur.", "true");
core::Property RouteOnAttribute::maliciousActivity(
    "maliciousActivity Attribute", "If the 'maliciousActivity' attribute is set to 'true', then will route FlowFile to MaliciousActivity relationship.  Value of property should match value of attribute for matching to occur", "true");
core::Property RouteOnAttribute::benignTraffic(
    "benignTraffic Attribute", "If the 'benignTraffic' attribute is set to 'true', then will route FlowFile to BenignTraffic relationship.  Value of property should match value of attribute for matching to occur.", "true");


core::Relationship RouteOnAttribute::MalwareDetected(
    "malwareDetected", "Flowfile will be routed to this relationship when malware is detected on the flowfile.");
core::Relationship RouteOnAttribute::MaliciousActivity(
    "maliciousActivity", "Flowfile will be routed to this relationship when maliciousActivity is indicated on the flowfile.");
core::Relationship RouteOnAttribute::BenignTraffic(
    "benignTraffic", "Flowfile will be routed to this relationship when traffic is indicated as benign on the flowfile.");
core::Relationship RouteOnAttribute::Unmatched(
    "Unmatched", "Flowfile will be routed to this relationship when the flowfile does not meet any of the other designed indicators.");

const char* RouteOnAttribute::MALWARE_DETECTED_ATTRIBUTE_KEY_NAME = "malwareDetected";
const char* RouteOnAttribute::MALICIOUS_ACTIVITY_ATTRIBUTE_KEY_NAME = "maliciousActivity";
const char* RouteOnAttribute::BENIGN_TRAFFIC_ATTRIBUTE_KEY_NAME = "benignTraffic";


void RouteOnAttribute::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(malwareDetected);
  properties.insert(maliciousActivity);
  properties.insert(benignTraffic);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(MalwareDetected);
  relationships.insert(MaliciousActivity);
  relationships.insert(BenignTraffic);
  relationships.insert(Unmatched);
  setSupportedRelationships(relationships);
}

void RouteOnAttribute::onTrigger(core::ProcessContext *context,
                             core::ProcessSession *session) {
  
  std::shared_ptr<core::FlowFile> flow = session->get();

  if (!flow)
    return;

  std::map<std::string, std::string>::iterator it;
  for (it = attrs.begin(); it != attrs.end(); it++) {
    
    std::string key = it->first;
    std::string value = it->second;
    std::string propertyValue;
    std::ostringstream indicatorStream;

    if(key == MALWARE_DETECTED_ATTRIBUTE_KEY_NAME){
    	indicatorStream << value;
	std::string output = indicatorStream.str();

    	if (context->getProperty(malwareDetected.getName(), propertyValue)) {
		if(propertyValue == output){
			session->transfer(flow, MalwareDetected);	
		} 		
    	}
    }
    else if(key == MALICIOUS_ACTIVITY_ATTRIBUTE_KEY_NAME){
        indicatorStream << value;
        std::string output = indicatorStream.str();

        if (context->getProperty(maliciousActivity.getName(), propertyValue)) {
                if(propertyValue == output){
                        session->transfer(flow, MaliciousActivity);
                }
        }
    }
    else if(key == BENIGN_TRAFFIC_ATTRIBUTE_KEY_NAME){
        indicatorStream << value;
        std::string output = indicatorStream.str();

        if (context->getProperty(benignTraffic.getName(), propertyValue)) {
                if(propertyValue == output){
                        session->transfer(flow, BenignTraffic);
                }
        }
    }
    else{
    	session->transfer(flow, Unmatched);
    }


  }

}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
