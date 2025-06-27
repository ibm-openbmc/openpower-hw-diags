// This simply does:
//  - Clone `openbmc-build-scripts` (from GitHub).
//  - Clone `openpower-hw-diags` (this repo, from GHE).
//  - Run the CI script from `openbmc-build-scripts` on `openpower-hw-diags`.

// IMPORTANT:
// The CI script expects:
//  - `WORKSPACE` to be set to the directory that contains both repositories.
//    Fortunately, this is already set by Jenkins.
//  - `UNIT_TEST_PKG` to be set to the name of the target repository to test.
//  - The directory containing the CI script repository must be named
//    `openbmc-build-scripts`.

pipeline
{
    agent
    {
        // TODO: Need to verify with this is the correct label to use.
        node { label 'general-docker-ci' }
    }
    options
    {
        timeout(time: 1, unit: 'HOURS')
        ansiColor('xterm')
        // TODO: Need a Jenkins plugin installed for this.
        //timestamps()

        // Checkout this repository. See notes above regarding
        // subdirectory requirement.
        checkoutToSubdirectory('openpower-hw-diags')
    }
    stages
    {
        stage('bmc-ci')
        {
            when
            {
                changeRequest() // only on pull requests
            }
            steps
            {
                // Checkout the CI script repository. See notes above regarding
                // subdirectory requirement.
                dir('openbmc-build-scripts')
                {
                    checkout scmGit(
                        branches: [[name: 'master']],
                        extensions: [ cloneOption(shallow: true) ],
                        userRemoteConfigs: [[
                            url: 'https://github.com/openbmc/openbmc-build-scripts.git',
                        ]]
                    )
                }

                // Run the CI script.
                sh('UNIT_TEST_PKG=openpower-hw-diags openbmc-build-scripts/run-unit-test-docker.sh')
            }
        }
    }
    post
    {
        always { cleanWs() }
    }
}
