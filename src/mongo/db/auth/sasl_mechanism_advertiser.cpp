/**
 * Copyright (C) 2018 MongoDB Inc.
 *
 * This program is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, the copyright holders give permission to link the
 * code of portions of this program with the OpenSSL library under certain
 * conditions as described in each individual source file and distribute
 * linked combinations including the program with the OpenSSL library. You
 * must comply with the GNU Affero General Public License in all respects for
 * all of the code used other than as permitted herein. If you modify file(s)
 * with this exception, you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version. If you delete this
 * exception statement from all source files in the program, then also delete
 * it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/db/auth/sasl_mechanism_advertiser.h"

#include "mongo/crypto/sha1_block.h"
#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/sasl_options.h"
#include "mongo/db/auth/user.h"

namespace mongo {

namespace {
void appendMechanismIfSupported(StringData mechanism, BSONArrayBuilder* builder) {
    const auto& globalMechanisms = saslGlobalParams.authenticationMechanisms;
    if (std::find(globalMechanisms.begin(), globalMechanisms.end(), mechanism) !=
        globalMechanisms.end()) {
        (*builder) << mechanism;
    }
}
}  // namespace


void SASLMechanismAdvertiser::advertise(OperationContext* opCtx,
                                        const BSONObj& cmdObj,
                                        BSONObjBuilder* result) {
    BSONElement saslSupportedMechs = cmdObj["saslSupportedMechs"];
    if (saslSupportedMechs.type() == BSONType::String) {
        AuthorizationManager* authManager = AuthorizationManager::get(opCtx->getServiceContext());

        UserName userName = uassertStatusOK(UserName::parse(saslSupportedMechs.String()));

        User* userObj;
        Status status = authManager->acquireUser(opCtx, userName, &userObj);
        uassertStatusOK(status);
        auto credentials = userObj->getCredentials();
        authManager->releaseUser(userObj);

        BSONArrayBuilder mechanismsBuilder;
        if (credentials.isExternal) {
            for (const StringData& userMechanism : {"GSSAPI", "PLAIN"}) {
                appendMechanismIfSupported(userMechanism, &mechanismsBuilder);
            }
        } else if (credentials.scram<SHA1Block>().isValid()) {
            appendMechanismIfSupported("SCRAM-SHA-1", &mechanismsBuilder);
        }

        result->appendArray("saslSupportedMechs", mechanismsBuilder.arr());
    }
}


}  // namespace mongo