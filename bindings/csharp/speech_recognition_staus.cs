//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

namespace Microsoft.CognitiveServices.Speech
{
    /// <summary>
    /// Defines speech recognition status.
    /// </summary>
    public enum SpeechRecognitionStatus
    {
        Recognized,
        IntermediateResult,
        NoMatch,
        Canceled,
        OtherRecognizer
    }
}