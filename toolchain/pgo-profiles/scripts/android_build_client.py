import apiclient.discovery
import apiclient.http
from oauth2client import client as oauth2_client
import io
import os

ANDROID_BUILD_API_SCOPE = (
    'https://www.googleapis.com/auth/androidbuild.internal')
ANDROID_BUILD_API_NAME = 'androidbuildinternal'
ANDROID_BUILD_API_VERSION = 'v2beta1'
TRADEFED_KEY_FILE = '/google/data/ro/teams/tradefed/configs/tradefed.json'
CHUNK_SIZE = 10 * 1024 * 1024  # 10M

ANDROID_PGO_BUILD = 'pgo-taimen-config1'


class AndroidBuildClient(object):

    def __init__(self):
        creds = oauth2_client.GoogleCredentials.from_stream(TRADEFED_KEY_FILE)
        self.creds = creds.create_scoped([ANDROID_BUILD_API_SCOPE])

        self.client = apiclient.discovery.build(
            ANDROID_BUILD_API_NAME,
            ANDROID_BUILD_API_VERSION,
            credentials=creds,
            discoveryServiceUrl=apiclient.discovery.DISCOVERY_URI)

    # Get the latest test invocation for a given test_tag for a given build.
    def get_invocation_id(self, build, test_tag):
        request = self.client.testresult().list(
            buildId=build, target=ANDROID_PGO_BUILD, attemptId='latest')

        response = request.execute()
        testResultWithTag = [
            r for r in response['testResults'] if r['testTag'] == test_tag
        ]
        if len(testResultWithTag) != 1:
            raise RuntimeError(
                'Expected one test with tag {} for build {}.  Found {}.  Full response is {}'
                .format(test_tag, build, len(testResultWithTag), response))
        return testResultWithTag[0]['id']

    # Get the full artifact name for the zipped PGO profiles
    # (_data_local_tmp_pgo_<hash>.zip) for a given <build, test_tag,
    # invocation_id>.
    def get_test_artifact_name(self, build, test_tag, invocation_id):
        request = self.client.testartifact().list(
            buildType='submitted',
            buildId=build,
            target=ANDROID_PGO_BUILD,
            attemptId='latest',
            testResultId=invocation_id,
            maxResults=100)

        response = request.execute()
        profile_zip = [
            f for f in response['test_artifacts']
            if f['name'].endswith('zip') and '_data_local_tmp_pgo_' in f['name']
        ]
        if len(profile_zip) != 1:
            raise RuntimeError(
                'Expected one matching zipfile for invocation {} of {} for build {}.  Found {} ({})'
                .format(invocation_id, test_tag, build, len(profile_zip),
                        ', '.join(profile_zip)))
        return profile_zip[0]['name']

    # Download the zipped PGO profiles for a given <build, test_tag,
    # invocation_id, artifact_name> into <output_zip>.
    def download_test_artifact(self, build, invocation_id, artifact_name,
                               output_zip):
        request = self.client.testartifact().get_media(
            buildType='submitted',
            buildId=build,
            target=ANDROID_PGO_BUILD,
            attemptId='latest',
            testResultId=invocation_id,
            resourceId=artifact_name)

        f = io.FileIO(output_zip, 'wb')
        try:
            downloader = apiclient.http.MediaIoBaseDownload(
                f, request, chunksize=CHUNK_SIZE)
            done = False
            while not done:
                status, done = downloader.next_chunk()
        except apiclient.errors.HttpError as e:
            if e.resp.status == 404:
                raise RuntimeError(
                    'Artifact {} does not exist for invocation {} for build {}.'
                    .format(artifact_name, invocation_id, build))

    # For a <build, test_tag>, find the invocation_id, artifact_name and
    # download the artifact into <output_dir>/pgo_profiles.zip.
    def download_pgo_zip(self, build, test_tag, output_dir):
        output_zip = os.path.join(output_dir, 'pgo_profiles.zip')

        invocation_id = self.get_invocation_id(build, test_tag)
        artifact_name = self.get_test_artifact_name(build, test_tag,
                                                    invocation_id)
        self.download_test_artifact(build, invocation_id, artifact_name,
                                    output_zip)
        return output_zip
