//
// copyright (c) Microsoft. All rights reserved.
// licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
import { SpeechRecognitionResult } from "./Exports";

/**
 * Defines contents of speech intermediate/final result event.
 * @class
 */
export class SpeechRecognitionResultEventArgs {
    /**
     * Specifies the recognition result.
     * @property
     * @returns the recognition result.
     */
    public result: SpeechRecognitionResult;

    /**
     * Specifies the session identifier.
     * @property
     * @returns the session identifier.
     */
    public sessionId: string;
}