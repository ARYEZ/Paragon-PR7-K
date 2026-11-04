FetchContent_Declare(catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(catch2)

add_executable(ParagonX1Tests
    TestMain.cpp
    SynthTests.cpp
    WavetableTests.cpp
    ShapeTests.cpp
    ModulationTests.cpp
    FftTests.cpp
    FreeformTests.cpp
    ShaperTests.cpp
    VoiceFilterTests.cpp
    MimicTests.cpp
    SfzTests.cpp
    LibraryTests.cpp
    MatrixTests.cpp
    EnvelopeModTests.cpp
    PitchModuleTests.cpp
    StepSeqTests.cpp
    TravelTests.cpp
    ModRoutingTests.cpp
    ArpTests.cpp
    ArpChordTests.cpp
    StackSizeTests.cpp
    ParameterIdTests.cpp
    ProcessorTests.cpp
    RigTests.cpp
    ExpansionTests.cpp
    EffectModTests.cpp
    UiTests.cpp
    UndoTests.cpp
    EffectTests.cpp
    EqTests.cpp
    LimiterTests.cpp
    MasterLimiterTests.cpp
    RackTests.cpp
    DelayReverbTests.cpp
    ModFxTests.cpp
    ModFxStereoTests.cpp
    WahTests.cpp
    WahPluginTests.cpp
    EchoesTests.cpp
    CrackleTests.cpp
    CracklePluginTests.cpp
    DownpourTests.cpp
    DownpourPluginTests.cpp
    SiroccoTests.cpp
    SiroccoPluginTests.cpp
    InfernalTests.cpp
    InfernalPluginTests.cpp
    AfterglowTests.cpp
    AfterglowPluginTests.cpp
    LightTrailTests.cpp
    LightTrailPluginTests.cpp
    BlackHoleTests.cpp
    BlackHolePluginTests.cpp
    DilationTests.cpp
    DilationPluginTests.cpp
    RemnantTests.cpp
    RemnantPluginTests.cpp
    ProximityTests.cpp
    ProximityPluginTests.cpp
    HazeTests.cpp
    HazePluginTests.cpp
    SpectreTests.cpp
    SpectrePluginTests.cpp
    LeviathanTests.cpp
    LeviathanPluginTests.cpp
    NuclearSpringTests.cpp
    NuclearSpringPluginTests.cpp
    LightSpeedTests.cpp
    LightSpeedPluginTests.cpp
    SpeedTests.cpp
    EchoesPluginTests.cpp
    DriveGlitchTests.cpp
    InsertTests.cpp
    SendTests.cpp
    CompressorTests.cpp
    DominionTests.cpp
    MultibandTests.cpp
    EditorSnapshot.cpp)

# Link the plugin's shared code (processor, editor, JUCE modules) so the
# tests exercise exactly the code that ships in the VST3. JUCE modules are
# linked privately into that target, so borrow its include paths and defines
# (as SYSTEM, so warnings inside JUCE headers don't flood the test build).
target_link_libraries(ParagonX1Tests PRIVATE ParagonX1 Catch2::Catch2)
target_include_directories(ParagonX1Tests SYSTEM PRIVATE $<TARGET_PROPERTY:ParagonX1,INCLUDE_DIRECTORIES>)
target_compile_definitions(ParagonX1Tests PRIVATE
    $<TARGET_PROPERTY:ParagonX1,COMPILE_DEFINITIONS>)

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(ParagonX1Tests)
